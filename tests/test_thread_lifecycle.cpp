#include "TestRuntime.hpp"
#include <ESPressio_Thread.hpp>
#include <cassert>
#include <new>
using namespace ESPressio::Threads;
static std::atomic<bool> denyHeap{false};
void* operator new(std::size_t n) { if (denyHeap) std::abort(); if (void* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
class Probe final : public Thread {
public:
    std::atomic<unsigned> Loops{0},Rollbacks{0},Terminations{0};
    unsigned ImmediateIterations=0;
    bool FailInitialization=false,EndDuringInitialization=false,FailApplication=false,FailTeardown=false,SelfShutdown=false;
    std::atomic<ThreadStatus> SelfStatus{ThreadStatus::Success};
    ~Probe() override { assert(Shutdown()==ThreadStatus::Success); }
protected:
    void OnInitialization() override { if (EndDuringInitialization) Terminate(); if (FailInitialization) throw 42; }
    void OnInitializationRollback() noexcept override { ++Rollbacks; }
    ThreadWorkDisposition OnLoop() override {
        ++Loops;
        if (SelfShutdown) SelfStatus=Shutdown();
        if (FailApplication) throw 7;
        if (ImmediateIterations) { if (Loops>=ImmediateIterations) Terminate(); return ThreadWorkDisposition::ImmediateWorkRemaining; }
        return ThreadWorkDisposition::IdleReady;
    }
    void OnTermination() override { ++Terminations; if (FailTeardown) throw 9; }
};
int main() {
    HostRuntime runtime;
    {
        Probe probe;
        assert(probe.Start()==ThreadStatus::InvalidState);
        runtime.FailSignal=true; assert(probe.Initialize()==ThreadStatus::SignalUnavailable); runtime.FailSignal=false;
        probe.FailInitialization=true; assert(probe.Initialize()==ThreadStatus::InitializationFailed);
        assert(probe.Rollbacks==1 && probe.GetThreadState()==ThreadState::Uninitialized && runtime.Created==0);
        probe.FailInitialization=false; probe.EndDuringInitialization=true;
        assert(probe.Initialize()==ThreadStatus::InvalidState); probe.EndDuringInitialization=false;
        runtime.FailExecution=true; assert(probe.Initialize()==ThreadStatus::TaskCreationFailed); runtime.FailExecution=false;
        assert(probe.Initialize()==ThreadStatus::Success);
        const auto signals=runtime.Signals.load();
        assert(runtime.Created==1 && probe.Loops==0);
        assert(probe.Configure({})==ThreadStatus::InvalidState);
        assert(probe.Start()==ThreadStatus::Success); Await([&]{return probe.Loops==1 && runtime.Waits>=2;});
        const auto iterations=probe.Loops.load();
        const auto waits=runtime.Waits.load();
        probe.Wake(); probe.WakeFromInterrupt(); Await([&]{return runtime.Waits>waits;});
        assert(probe.Loops==iterations); // A wake is never a counted application callback.
        denyHeap=true;
        probe.RequestLoop(); Await([&]{return probe.Loops==2;});
        assert(probe.Pause()==ThreadStatus::Success);
        probe.RequestLoop(); assert(probe.Start()==ThreadStatus::Success);
        Await([&]{return probe.Loops>=3;});
        probe.Terminate(); assert(probe.Shutdown()==ThreadStatus::Success);
        denyHeap=false;
        assert(runtime.Joined==1 && runtime.Signals==signals && probe.Terminations==1);
        assert(probe.GetDiagnostics().Failure.Phase==ThreadFailurePhase::None);
        assert(probe.Initialize()==ThreadStatus::Success && runtime.Signals==signals);
        probe.SelfShutdown=true; assert(probe.Start()==ThreadStatus::Success);
        Await([&]{return probe.GetThreadState()==ThreadState::Terminated;});
        assert(probe.SelfStatus==ThreadStatus::SelfJoin); assert(probe.Shutdown()==ThreadStatus::Success);
    }
    {
        Probe probe; probe.FailApplication=true; probe.FailTeardown=true;
        assert(probe.Initialize()==ThreadStatus::Success && probe.Start()==ThreadStatus::Success);
        Await([&]{return probe.GetThreadState()==ThreadState::Terminated;});
        const auto fault=probe.GetDiagnostics().Failure;
        assert(fault.Phase==ThreadFailurePhase::Application);
        try { std::rethrow_exception(fault.Cause); } catch (int value) { assert(value==7); }
        assert(probe.Loops==1 && probe.Terminations==1); assert(probe.Shutdown()==ThreadStatus::Success);
    }
    {
        Probe probe; ThreadConfiguration configuration; configuration.Execution.StackSize=128;
        assert(probe.Configure(configuration)==ThreadStatus::Success);
        assert(probe.Initialize()==ThreadStatus::StackTooSmall);
    }
    {
        Probe probe; probe.ImmediateIterations=100;
        assert(probe.Initialize()==ThreadStatus::Success && probe.Start()==ThreadStatus::Success);
        Await([&]{return probe.GetThreadState()==ThreadState::Terminated;});
        assert(probe.Loops==100 && probe.Shutdown()==ThreadStatus::Success);
    }
    assert(runtime.Created==runtime.Joined);
}
