#pragma once
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <ESPressio_Synchronization.hpp>
#include <ESPressio_SystemPlatformClock.hpp>
#include <ESPressio_TaskRuntime.hpp>
#include "ESPressio_IThread.hpp"
#include "ESPressio_ThreadCapability.hpp"
namespace ESPressio::Threads {
/// <summary>One persistent, cooperatively joined task and one common latched work signal.</summary>
/// <remarks>The concrete owner must Shutdown before destroying members accessed by OnLoop. The host additionally
/// joins before destroying capabilities. Providers and the clock must outlive this object; diagnostic Name must outlive the initialized task.
/// Wake callers must retain the Thread object's lifetime; capability quiescence closes external registrations.</remarks>
class Thread : public IThread {
    using Mutex=System::Synchronization::Mutex;
    mutable Mutex _control,_ownerOperation,_failureMutex;
    std::atomic<ThreadState> _state{ThreadState::Uninitialized};
    std::atomic<Task::TaskHandle> _handle{System::Execution::InvalidExecutionHandle};
    std::unique_ptr<System::Synchronization::ISignal> _wake;
    std::atomic<System::Synchronization::ISignal*> _publishedWake{nullptr};
    System::Execution::IExecutionProvider* _provider=nullptr;
    System::Clock::IMonotonicClock* _clock=nullptr;
    ThreadConfiguration _configuration{};
    ThreadFailure _failure{};
    std::uint64_t _activation=0;
    std::atomic<bool> _published{false},_applicationRequested{false};
    std::atomic<std::uint64_t> _cycles{0},_iterations{0},_waits{0};
    bool _needsTime=false;
    void RecordFailure(ThreadFailurePhase phase,std::exception_ptr cause={}) noexcept {
        std::lock_guard<Mutex> lock(_failureMutex);
        if (_failure.Phase==ThreadFailurePhase::None) _failure={phase,std::move(cause),Now()};
    }
    ThreadCycleContext Context() noexcept { return {_needsTime ? Now() : 0,HostServices()}; }
    static void Entry(void* value) noexcept { static_cast<Thread*>(value)->Run(); }
    bool ClaimQuantum(std::uint64_t epoch) {
        // This claim linearizes the start of a bounded quantum against Pause/Terminate.
        // A claimed quantum is in flight and may finish; subsequent claims fail.
        std::lock_guard<Mutex> lock(_control);
        return _state.load(std::memory_order_relaxed)==ThreadState::Running && epoch==_activation;
    }
    void Wait(ThreadDeadline deadline) noexcept {
        std::uint32_t milliseconds=System::Synchronization::WaitForever;
        if (deadline.Present) {
            const auto now=Now();
            if (deadline.Due(now)) return;
            const auto remaining=deadline.MonotonicNanoseconds-now;
            const auto rounded=remaining/1000000+(remaining%1000000!=0);
            milliseconds=static_cast<std::uint32_t>(std::min<std::uint64_t>(rounded,UINT32_MAX-1));
        }
        ++_waits;
        const auto result=_wake->Wait(milliseconds);
        if (!result && result.Status!=System::PlatformStatus::Timeout) {
            RecordFailure(ThreadFailurePhase::Provider); Terminate();
        }
    }
    void Run() noexcept {
        ThreadFailurePhase phase=ThreadFailurePhase::Lifecycle;
        bool activated=false;
        std::uint64_t epoch=0;
        try {
            while (!_published.load(std::memory_order_acquire)) Wait({});
            for (;;) {
                auto state=_state.load(std::memory_order_acquire);
                if (state==ThreadState::Terminating || state==ThreadState::Terminated) break;
                auto context=Context();
                if (state!=ThreadState::Running) {
                    if (activated) {
                        phase=ThreadFailurePhase::Lifecycle;
                        (void)Host(ThreadHostOperation::Pause,context); activated=false;
                    }
                    // Never reset the latch between checking state/readiness and waiting.
                    Wait({}); continue;
                }
                std::uint64_t requestedEpoch;
                { std::lock_guard<Mutex> lock(_control); requestedEpoch=_activation; }
                if (!activated || epoch!=requestedEpoch) {
                    phase=ThreadFailurePhase::Lifecycle;
                    (void)Host(ThreadHostOperation::Activate,context);
                    activated=true; epoch=requestedEpoch; _applicationRequested.store(true,std::memory_order_release);
                    if (!ClaimQuantum(epoch)) continue;
                }
                ++_cycles;
                phase=ThreadFailurePhase::Capability;
                auto result=Host(ThreadHostOperation::Inspect,context);
                if (!result.Ready.ApplicationDeadlineDue && result.Ready.Immediate && ClaimQuantum(epoch)) {
                    context=Context();
                    result=Host(ThreadHostOperation::Inspect,context);
                    if (!result.Ready.ApplicationDeadlineDue) (void)Host(ThreadHostOperation::Service,context);
                    context=Context(); // One positive quantum may have crossed a temporal deadline.
                    result=Host(ThreadHostOperation::Inspect,context);
                }
                if (result.Ready.ApplicationEligible &&
                    (result.Ready.ApplicationDeadlineDue || _applicationRequested.load(std::memory_order_acquire)) && ClaimQuantum(epoch)) {
                    _applicationRequested.store(false,std::memory_order_release);
                    phase=ThreadFailurePhase::Capability;
                    if (!Host(ThreadHostOperation::BeforeApplication,context).Ready.ApplicationEligible) {
                        _applicationRequested.store(true,std::memory_order_release); continue;
                    }
                    phase=ThreadFailurePhase::Application;
                    const auto disposition=OnLoop(); ++_iterations;
                    if (disposition==ThreadWorkDisposition::ImmediateWorkRemaining)
                        _applicationRequested.store(true,std::memory_order_release);
                    phase=ThreadFailurePhase::Capability;
                    context=Context(); (void)Host(ThreadHostOperation::AfterApplication,context);
                }
                if (_state.load(std::memory_order_acquire)!=ThreadState::Running) continue;
                context=Context(); result=Host(ThreadHostOperation::Inspect,context);
                if (result.Ready.Immediate || result.Ready.Deadline.Due(context.Now) ||
                    (result.Ready.ApplicationEligible && _applicationRequested.load(std::memory_order_acquire))) continue;
                Wait(result.Ready.Deadline);
            }
        } catch (...) { RecordFailure(phase,std::current_exception()); Terminate(); }
        auto context=Context();
        try { (void)Host(ThreadHostOperation::Quiesce,context); }
        catch (...) { RecordFailure(ThreadFailurePhase::Teardown,std::current_exception()); }
        try { OnTermination(); }
        catch (...) { RecordFailure(ThreadFailurePhase::Teardown,std::current_exception()); }
        _state.store(ThreadState::Terminated,std::memory_order_release);
        // Return normally. System provider trampoline owns native exit and Join completion.
    }
protected:
    virtual ThreadWorkDisposition OnLoop() { return ThreadWorkDisposition::IdleReady; }
    virtual void OnInitialization() {}
    virtual void OnInitializationRollback() noexcept {}
    virtual void OnTermination() {}
    /// <summary>The sole neutral host override. Bare Thread never enumerates or knows domain capabilities.</summary>
    virtual ThreadHostResult Host(ThreadHostOperation,ThreadCycleContext&) { return {}; }
    ThreadHostServices HostServices() noexcept {
        return {this,[](void* p,bool interrupt) noexcept { return interrupt ? static_cast<Thread*>(p)->WakeFromInterrupt() : static_cast<Thread*>(p)->Wake(); },
            [](const void* p) noexcept { const auto s=static_cast<const Thread*>(p)->GetThreadState();
                return s==ThreadState::Initialized || s==ThreadState::Running || s==ThreadState::Paused; },
            [](const void* p) noexcept { return static_cast<const Thread*>(p)->Now(); },
            [](void* p) noexcept {
                auto& thread=*static_cast<Thread*>(p);
                if (!thread._control.try_lock()) return false;
                const auto state=thread.GetThreadState();
                if (state==ThreadState::Initialized || state==ThreadState::Running || state==ThreadState::Paused) return true;
                thread._control.unlock(); return false;
            },
            [](void* p) noexcept { static_cast<Thread*>(p)->_control.unlock(); }};
    }
public:
    static constexpr std::uint32_t RootFrameworkStackFloorBytes=1024;
    explicit Thread(const ThreadConfiguration& configuration={},System::Clock::IMonotonicClock* clock=nullptr)
        :_clock(clock),_configuration(configuration) {}
    Thread(const Thread&)=delete; Thread& operator=(const Thread&)=delete;
    ~Thread() override { if (Shutdown()!=ThreadStatus::Success) std::terminate(); }
    /// <summary>Monotonic scheduling only; the provider is frozen at initialization.</summary>
    std::uint64_t Now() const noexcept { return (_clock ? *_clock : System::Clock::Monotonic()).NowNanoseconds(); }
    ThreadState GetThreadState() const noexcept override { return _state.load(std::memory_order_acquire); }
    bool Wake() noexcept {
        auto* signal=_publishedWake.load(std::memory_order_acquire);
        return signal && static_cast<bool>(signal->Give());
    }
    bool WakeFromInterrupt() noexcept {
        auto* signal=_publishedWake.load(std::memory_order_acquire);
        return signal && static_cast<bool>(signal->GiveFromInterrupt());
    }
    /// <summary>Publishes ordinary application demand separately from capability work, then uses the common Wake.</summary>
    void RequestLoop() noexcept { _applicationRequested.store(true,std::memory_order_release); (void)Wake(); }
    ThreadStatus Configure(const ThreadConfiguration& configuration) {
        std::unique_lock<Mutex> owner(_ownerOperation,std::try_to_lock);
        if (!owner.owns_lock()) return ThreadStatus::Busy;
        if (_handle.load()!=System::Execution::InvalidExecutionHandle || GetThreadState()!=ThreadState::Uninitialized)
            return ThreadStatus::InvalidState;
        std::lock_guard<Mutex> lock(_control); _configuration=configuration; return ThreadStatus::Success;
    }
    ThreadResourceProfile GetResourceProfile() {
        std::lock_guard<Mutex> lock(_control);
        ThreadCycleContext context{0,HostServices()};
        auto result=Host(ThreadHostOperation::Resources,context).Resources;
        if (!result.ResidentBytes) { result.ResidentBytes=sizeof(Thread); result.ResidentAlignment=alignof(Thread); }
        result.FrameworkStackFloorBytes=std::max(result.FrameworkStackFloorBytes,RootFrameworkStackFloorBytes);
        result.FrameworkStackFloorBytes=std::max(result.FrameworkStackFloorBytes,_configuration.ApplicationStackFloorBytes);
        result.ConfiguredStackBytes=_configuration.Execution.StackSize;
        return result;
    }
    ThreadStatus Initialize() override {
        std::unique_lock<Mutex> owner(_ownerOperation,std::try_to_lock);
        if (!owner.owns_lock()) return ThreadStatus::Busy;
        if (_handle.load()!=System::Execution::InvalidExecutionHandle) return ThreadStatus::AlreadyInitialized;
        if (GetThreadState()!=ThreadState::Uninitialized) return ThreadStatus::InvalidState;
        { std::lock_guard<Mutex> lock(_control); _state.store(ThreadState::Initializing); }
        { std::lock_guard<Mutex> lock(_failureMutex); _failure={}; }
        if (!_clock) _clock=&System::Clock::Monotonic();
        _provider=&System::Execution::Provider();
        ThreadCycleContext context{Now(),HostServices()};
        bool applicationEntered=false,hostEntered=false;
        ThreadStatus status=ThreadStatus::Success;
        const bool hadWake=bool(_wake);
        try {
            const auto resources=GetResourceProfile();
            if (!_configuration.Execution.StackSize) status=ThreadStatus::InvalidConfiguration;
            else if (_configuration.Execution.StackSize<resources.FrameworkStackFloorBytes) status=ThreadStatus::StackTooSmall;
            if (status==ThreadStatus::Success && !_wake) {
                auto* signals=System::Synchronization::Provider();
                if (signals) _wake=signals->CreateBinarySignal();
                if (!_wake) status=ThreadStatus::SignalUnavailable;
            }
            if (status==ThreadStatus::Success) {
                applicationEntered=true; OnInitialization();
                hostEntered=true; status=Host(ThreadHostOperation::Initialize,context).Status;
            }
            if (status==ThreadStatus::Success) status=Host(ThreadHostOperation::FinalizeInitialization,context).Status;
            if (status==ThreadStatus::Success && GetThreadState()!=ThreadState::Initializing) status=ThreadStatus::InvalidState;
            if (status==ThreadStatus::Success) {
                _needsTime=Host(ThreadHostOperation::Resources,context).NeedsMonotonicTime;
                _published.store(false,std::memory_order_release);
                std::lock_guard<Mutex> publication(_control);
                if (GetThreadState()!=ThreadState::Initializing) throw ThreadStatus::InvalidState;
                const auto created=Task::TaskRuntime::CreateJoinable(&Entry,this,_configuration.Execution,*_provider);
                if (!created) {
                    status=created.Status==Task::TaskExecutionStatus::UnsupportedExecutionProvider ? ThreadStatus::UnsupportedProvider :
                        created.Status==Task::TaskExecutionStatus::InvalidConfiguration || created.Status==Task::TaskExecutionStatus::UnsupportedMemoryPolicy ?
                        ThreadStatus::InvalidConfiguration : ThreadStatus::TaskCreationFailed;
                } else {
                    _handle.store(created.Handle,std::memory_order_release);
                    _publishedWake.store(_wake.get(),std::memory_order_release);
                    _state.store(ThreadState::Initialized,std::memory_order_release);
                    _published.store(true,std::memory_order_release); (void)Wake();
                    return ThreadStatus::Success;
                }
            }
        } catch (...) { RecordFailure(ThreadFailurePhase::Initialization,std::current_exception()); status=ThreadStatus::InitializationFailed; }
        if (hostEntered) { try { (void)Host(ThreadHostOperation::RollbackInitialization,context); }
            catch (...) { RecordFailure(ThreadFailurePhase::Teardown,std::current_exception()); } }
        if (applicationEntered) OnInitializationRollback();
        if (!hadWake) _wake.reset();
        _state.store(ThreadState::Uninitialized,std::memory_order_release);
        return status;
    }
    /// <summary>Activates only an already-initialized frozen topology; first Start/each Resume starts a fresh epoch.</summary>
    ThreadStatus Start() override {
        std::lock_guard<Mutex> lock(_control);
        const auto state=GetThreadState();
        if (state!=ThreadState::Initialized && state!=ThreadState::Paused) return ThreadStatus::InvalidState;
        if (_activation==UINT64_MAX) return ThreadStatus::GenerationExhausted;
        ++_activation; _state.store(ThreadState::Running,std::memory_order_release); (void)Wake(); return ThreadStatus::Success;
    }
    ThreadStatus Pause() override {
        std::lock_guard<Mutex> lock(_control);
        if (GetThreadState()!=ThreadState::Running) return ThreadStatus::InvalidState;
        _state.store(ThreadState::Paused,std::memory_order_release); (void)Wake(); return ThreadStatus::Success;
    }
    /// <summary>Closes new quantum claims immediately; current bounded work completes and quiescence invokes no application drain.</summary>
    void Terminate() noexcept override {
        std::lock_guard<Mutex> lock(_control);
        const auto state=GetThreadState();
        if (state==ThreadState::Uninitialized || state==ThreadState::Terminated) return;
        _state.store(ThreadState::Terminating,std::memory_order_release); (void)Wake();
    }
    /// <summary>External owner joins native exit. A worker call requests termination and reports SelfJoin.</summary>
    ThreadStatus Shutdown() noexcept override {
        auto handle=_handle.load(std::memory_order_acquire);
        if (handle!=System::Execution::InvalidExecutionHandle && _provider && _provider->Current()==handle) {
            Terminate(); return ThreadStatus::SelfJoin;
        }
        std::unique_lock<Mutex> owner(_ownerOperation,std::try_to_lock);
        if (!owner.owns_lock()) return ThreadStatus::Busy;
        handle=_handle.load(std::memory_order_acquire);
        if (handle!=System::Execution::InvalidExecutionHandle) {
            Terminate();
            try { if (!_provider->Join(handle)) return ThreadStatus::JoinFailed; }
            catch (...) { RecordFailure(ThreadFailurePhase::Provider,std::current_exception()); return ThreadStatus::JoinFailed; }
            _handle.store(System::Execution::InvalidExecutionHandle,std::memory_order_release);
        }
        _state.store(ThreadState::Uninitialized,std::memory_order_release);
        // Keep the one lifetime-owned wake allocated after success: concurrent stale Wake is harmless,
        // and no signal pointer is reclaimed until owner-controlled object destruction.
        return ThreadStatus::Success;
    }
    ThreadDiagnostics GetDiagnostics() const override {
        ThreadDiagnostics result;
        result.State=GetThreadState(); result.ServiceCycles=_cycles.load(); result.ApplicationIterations=_iterations.load(); result.Waits=_waits.load();
        { std::lock_guard<Mutex> lock(_failureMutex); result.Failure=_failure; }
        std::unique_lock<Mutex> owner(_ownerOperation,std::try_to_lock);
        if (owner.owns_lock()) { const auto handle=_handle.load(); if (handle && _provider) {
            result.MinimumFreeStackBytes=_provider->MinimumFreeStackBytes(handle); result.StackTelemetryAvailable=true;
        } }
        return result;
    }
};
}
