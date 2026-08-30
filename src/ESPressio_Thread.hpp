#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Synchronization.hpp>
#include <ESPressio_Task.hpp>

#include "ESPressio_IThread.hpp"
#include "ESPressio_IThreadObserver.hpp"
#include "ESPressio_ThreadReleasePolicy.hpp"
#include "ESPressio_ThreadSafe.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

#ifndef ESPRESSIO_THREAD_DEFAULT_STACK_SIZE
#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 4000
#endif

namespace ESPressio {
namespace Threads {

class ThreadTerminationDispatcher;

/// <summary>Concrete managed task implementation providing the ESPressio IThread lifecycle and observer/callback surfaces.</summary>
/// <remarks>Thread owns an underlying platform task while initialized, serializes lifecycle transitions through ESPressio System synchronization, and defers task-exit finalization through ThreadTerminationDispatcher.</remarks>
class Thread : public IThread {
private:
    class LifecycleObservable final : public Observable::ThreadSafeObservable {
    private:
        template <typename TNotification>
        void Notify(TNotification notification) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IThreadObserver>([&](IThreadObserver* observer) {
                    try { notification(observer); } catch (...) {}
                });
            });
        }

    public:
        void NotifyStateChanged(Thread* thread, ThreadState oldState, ThreadState newState) {
            ExecuteNotification([&](NotificationContext& notification) {
                notification.WithObservers<IThreadObserver>([&](IThreadObserver* observer) {
                    try { observer->OnThreadStateChanged(thread, oldState, newState); } catch (...) {}
                    if (thread->GetThreadState() != newState) return;
                    try {
                        switch (newState) {
                            case ThreadState::Uninitialized: observer->OnThreadUninitialized(thread); break;
                            case ThreadState::Initialized: observer->OnThreadInitialized(thread); break;
                            case ThreadState::Running: observer->OnThreadStarted(thread); break;
                            case ThreadState::Paused: observer->OnThreadPaused(thread); break;
                            case ThreadState::Terminating: observer->OnThreadTerminationRequested(thread); break;
                            case ThreadState::Terminated: observer->OnThreadTerminated(thread); break;
                            case ThreadState::Destroyed: observer->OnThreadDestroyed(thread); break;
                        }
                    } catch (...) {}
                });
            });
        }

        void NotifyTaskExited(Thread* thread) {
            Notify([&](IThreadObserver* observer) { observer->OnThreadTaskExited(thread); });
        }

        void NotifyInitializationFailed(Thread* thread, ThreadInitializationStatus status) {
            Notify([&](IThreadObserver* observer) {
                observer->OnThreadInitializationFailed(thread, status);
            });
        }

        void NotifyExecutionFailed(Thread* thread, std::exception_ptr cause) {
            Notify([&](IThreadObserver* observer) {
                observer->OnThreadExecutionFailed(thread, cause);
            });
        }
    };

    enum class CleanupClaim : uint8_t {
        Available,
        Manual,
        Automatic
    };

    using TOnThreadEvent = std::function<void(IThread*)>;
    using TOnThreadStateChangeEvent = std::function<void(IThread*, ThreadState, ThreadState)>;
    using TOnThreadInitializationFailedEvent = std::function<void(IThread*, ThreadInitializationStatus)>;
    using TOnThreadExecutionFailedEvent = std::function<void(IThread*, std::exception_ptr)>;

    template<typename TCallback>
    using StableCallback = std::shared_ptr<const TCallback>;

    template<typename TCallback>
    static StableCallback<TCallback> MakeStableCallback(TCallback value) {
        if (!value) return nullptr;
        return System::Memory::MakeShared<
            TCallback,
            System::Memory::MemoryPolicy::ExternalPreferred
        >(std::move(value));
    }

    uint8_t _threadID = 0;

    std::atomic<ThreadState> _threadState{ThreadState::Uninitialized};
    std::atomic<bool> _freeOnTerminate{false};
    std::atomic<bool> _startOnInitialize{true};

    std::atomic<Task::TaskHandle> _taskHandle{System::Execution::InvalidExecutionHandle};
    std::atomic<Task::TaskHandle> _initializingTaskHandle{System::Execution::InvalidExecutionHandle};
    std::atomic<bool> _initializationInProgress{false};
    std::atomic<bool> _terminationDispatchPending{false};
    std::atomic<bool> _taskExitFinalizationStarted{false};
    std::atomic<CleanupClaim> _cleanupClaim{CleanupClaim::Available};

    std::unique_ptr<System::Synchronization::ISignal> _taskExited;
    std::unique_ptr<System::Synchronization::ISignal> _taskStartGate;

    mutable System::Synchronization::Mutex _taskConfigurationMutex;
    mutable System::Synchronization::RecursiveMutex _stateTransitionMutex;

    std::atomic<uint32_t> _stackSize{ESPRESSIO_THREAD_DEFAULT_STACK_SIZE};
    std::atomic<unsigned int> _priority{2};
    std::atomic<int> _coreID{0};

    std::shared_ptr<LifecycleObservable> _lifecycleObservable;

    mutable System::Synchronization::Mutex _callbackMutex;
    StableCallback<TOnThreadEvent> _onDestroy;
    StableCallback<TOnThreadEvent> _onInitialize;
    StableCallback<TOnThreadEvent> _onStart;
    StableCallback<TOnThreadEvent> _onPause;
    StableCallback<TOnThreadEvent> _onTerminate;
    StableCallback<TOnThreadEvent> _onTerminated;
    StableCallback<TOnThreadInitializationFailedEvent> _onInitializationFailed;
    StableCallback<TOnThreadExecutionFailedEvent> _onExecutionFailed;
    StableCallback<TOnThreadStateChangeEvent> _onStateChange;

    bool _isValidThreadStateTransition(ThreadState oldState, ThreadState newState) const noexcept {
        if (oldState == newState) return false;
        if (newState == ThreadState::Destroyed) return oldState != ThreadState::Destroyed;

        switch (oldState) {
            case ThreadState::Uninitialized:
                return newState == ThreadState::Initialized || newState == ThreadState::Terminating;
            case ThreadState::Initialized:
                return newState == ThreadState::Running || newState == ThreadState::Terminating;
            case ThreadState::Running:
                return newState == ThreadState::Paused || newState == ThreadState::Terminating;
            case ThreadState::Paused:
                return newState == ThreadState::Running || newState == ThreadState::Terminating;
            case ThreadState::Terminating:
                return newState == ThreadState::Terminated;
            case ThreadState::Terminated:
                return newState == ThreadState::Uninitialized;
            case ThreadState::Destroyed:
                return false;
        }
        return false;
    }

    void _deleteTask() {
        const auto handle = _taskHandle.exchange(
            System::Execution::InvalidExecutionHandle,
            std::memory_order_acq_rel
        );
        if (handle != System::Execution::InvalidExecutionHandle) {
            Task::TaskRuntime::Delete(handle);
        }
    }

    static void _requestGarbageCollection();
    static bool _isTerminationDispatcherAvailable();
    static bool _isCurrentTerminationDispatcherTask();
    static bool _queueTerminationDispatch(Thread* thread);
    void _dispatchTermination();

    bool _beginTaskExitFinalization() noexcept {
        bool expected = false;
        return _taskExitFinalizationStarted.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    bool _queueTaskExitFinalization() noexcept {
        _terminationDispatchPending.store(true, std::memory_order_release);
        if (_queueTerminationDispatch(this)) return true;
        _terminationDispatchPending.store(false, std::memory_order_release);
        return false;
    }

    void _finalizeStoppedTaskExit() noexcept {
        if (_beginTaskExitFinalization()) _queueTaskExitFinalization();
    }

    [[noreturn]] void _finalizeCurrentTaskExit() noexcept {
        if (_beginTaskExitFinalization()) {
            try { TrySetThreadState(ThreadState::Terminating, ThreadState::Terminated); } catch (...) {}
            _queueTaskExitFinalization();
        }

        Task::TaskRuntime::Suspend(System::Execution::InvalidExecutionHandle);
        for (;;) {
            Task::TaskRuntime::SleepMilliseconds(1000);
        }
    }

    void _dispatchExecutionFailed(std::exception_ptr cause) noexcept {
        const std::exception_ptr executionFailure =
            std::make_exception_ptr(ThreadExecutionException(std::move(cause)));

        try {
            StableCallback<TOnThreadExecutionFailedEvent> callback;
            {
                std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex);
                callback = _onExecutionFailed;
            }
            if (callback != nullptr) (*callback)(this, executionFailure);
        } catch (...) {}

        try { _lifecycleObservable->NotifyExecutionFailed(this, executionFailure); } catch (...) {}
    }

    void _waitForTerminationDispatch() {
        if (!_terminationDispatchPending.load(std::memory_order_acquire)) return;
        if (_isCurrentTerminationDispatcherTask()) return;

        while (_terminationDispatchPending.load(std::memory_order_acquire)) {
            Task::TaskRuntime::SleepMilliseconds(1);
        }
    }

    void _loop() {
        for (;;) {
            switch (_threadState.load(std::memory_order_acquire)) {
                case ThreadState::Paused:
                case ThreadState::Initialized:
                case ThreadState::Uninitialized:
                    Task::TaskRuntime::SleepMilliseconds(1);
                    break;
                case ThreadState::Running:
                    OnLoop();
                    break;
                case ThreadState::Terminating:
                case ThreadState::Terminated:
                case ThreadState::Destroyed:
                    return;
            }
        }
    }

    void _dispatchThreadStateChange(ThreadState oldState, ThreadState newState) {
        StableCallback<TOnThreadStateChangeEvent> onStateChange;
        StableCallback<TOnThreadEvent> onThreadEvent;
        bool callbackFailed = false;
        {
            std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex);
            onStateChange = _onStateChange;
            switch (newState) {
                case ThreadState::Terminated: onThreadEvent = _onTerminate; break;
                case ThreadState::Paused: onThreadEvent = _onPause; break;
                case ThreadState::Running: onThreadEvent = _onStart; break;
                case ThreadState::Initialized: onThreadEvent = _onInitialize; break;
                default: break;
            }
        }

        if (onStateChange != nullptr) {
            try { (*onStateChange)(this, oldState, newState); } catch (...) { callbackFailed = true; }
        }

        if (onThreadEvent != nullptr && GetThreadState() == newState) {
            try { (*onThreadEvent)(this); } catch (...) { callbackFailed = true; }
        }

        try { _lifecycleObservable->NotifyStateChanged(this, oldState, newState); }
        catch (...) { callbackFailed = true; }

        if (
            callbackFailed &&
            _initializationInProgress.load(std::memory_order_acquire) &&
            _initializingTaskHandle.load(std::memory_order_acquire) == Task::TaskRuntime::Current()
        ) {
            throw std::runtime_error("Thread initialization lifecycle callback failed");
        }
    }

protected:
    /// <summary>Performs one iteration of user thread work while the Thread is in the Running state.</summary>
    virtual void OnLoop() {
        Task::TaskRuntime::SleepMilliseconds(1);
    }

    /// <summary>Hook invoked during task initialization before the Thread enters Initialized state.</summary>
    virtual void OnInitialization() {}

    /// <summary>Transitions the Thread to a valid new lifecycle state and dispatches associated callbacks/observer notifications.</summary>
    void SetThreadState(ThreadState state) {
        std::lock_guard<System::Synchronization::RecursiveMutex> transitionLock(_stateTransitionMutex);
        const ThreadState oldState = _threadState.load(std::memory_order_acquire);
        if (!_isValidThreadStateTransition(oldState, state)) return;
        _threadState.store(state, std::memory_order_release);
        _dispatchThreadStateChange(oldState, state);
    }

    /// <summary>Atomically performs a lifecycle transition only when the current state matches the expected state.</summary>
    bool TrySetThreadState(ThreadState expectedState, ThreadState newState) {
        std::lock_guard<System::Synchronization::RecursiveMutex> transitionLock(_stateTransitionMutex);
        const ThreadState currentState = _threadState.load(std::memory_order_acquire);
        if (
            currentState != expectedState ||
            !_isValidThreadStateTransition(currentState, newState)
        ) return false;
        _threadState.store(newState, std::memory_order_release);
        _dispatchThreadStateChange(expectedState, newState);
        return true;
    }

public:
    friend class ThreadTerminationDispatcher;

    Thread();

    explicit Thread(ThreadReleasePolicy releasePolicy) : Thread() {
        SetFreeOnTerminate(releasePolicy == ThreadReleasePolicy::ReleaseOnTerminate);
    }

    virtual ~Thread();

    void GarbageCollect();

    Observable::ObserverHandlePtr RegisterThreadObserver(IThreadObserver* observer) {
        return _lifecycleObservable->RegisterObserverAs<IThreadObserver>(observer);
    }

    void UnregisterThreadObserver(IThreadObserver* observer) {
        _lifecycleObservable->UnregisterObserver(observer);
    }

    void Shutdown() {
        CleanupClaim expectedClaim = CleanupClaim::Available;
        _cleanupClaim.compare_exchange_strong(
            expectedClaim,
            CleanupClaim::Manual,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );

        SetFreeOnTerminate(false);

        const auto handle = _taskHandle.load(std::memory_order_acquire);
        const auto currentTask = Task::TaskRuntime::Current();

        if (
            _initializationInProgress.load(std::memory_order_acquire) &&
            _initializingTaskHandle.load(std::memory_order_acquire) == currentTask
        ) {
            Terminate();
            return;
        }

        if (handle == System::Execution::InvalidExecutionHandle) {
            if (
                GetThreadState() != ThreadState::Terminated &&
                GetThreadState() != ThreadState::Destroyed
            ) {
                Terminate();
                SetThreadState(ThreadState::Terminated);
            }
            _waitForTerminationDispatch();
            return;
        }

        if (handle == currentTask) {
            Terminate();
            return;
        }

        Terminate();
        if (_taskExited != nullptr) (void)_taskExited->Wait();
        _waitForTerminationDispatch();
    }

private:
    static void _taskEntry(void* parameter) {
        Thread* instance = static_cast<Thread*>(parameter);

        if (instance != nullptr) {
            if (instance->_taskStartGate == nullptr || !instance->_taskStartGate->Wait()) {
                instance->_finalizeCurrentTaskExit();
            }
            try {
                instance->_loop();
            } catch (...) {
                instance->_dispatchExecutionFailed(std::current_exception());
                instance->Terminate();
            }
            instance->_finalizeCurrentTaskExit();
        }

        Task::TaskRuntime::Delete(System::Execution::InvalidExecutionHandle);
    }

    ThreadInitializationStatus _initialize() {
        if (_taskExited == nullptr || _taskStartGate == nullptr) {
            return ThreadInitializationStatus::ExitSignalUnavailable;
        }
        if (
            _taskHandle.load(std::memory_order_acquire) !=
            System::Execution::InvalidExecutionHandle
        ) {
            return ThreadInitializationStatus::AlreadyInitialized;
        }
        if (_terminationDispatchPending.load(std::memory_order_acquire)) {
            return ThreadInitializationStatus::TerminationDispatchPending;
        }
        if (!_isTerminationDispatcherAvailable()) {
            return ThreadInitializationStatus::TerminationDispatcherUnavailable;
        }

        const ThreadState initialState = GetThreadState();
        if (initialState == ThreadState::Terminated) {
            if (!TrySetThreadState(ThreadState::Terminated, ThreadState::Uninitialized)) {
                return ThreadInitializationStatus::InvalidState;
            }
        } else if (initialState != ThreadState::Uninitialized) {
            return ThreadInitializationStatus::InvalidState;
        }

        std::unique_lock<System::Synchronization::Mutex> configurationLock(_taskConfigurationMutex);
        if (
            _taskHandle.load(std::memory_order_acquire) !=
            System::Execution::InvalidExecutionHandle
        ) {
            return ThreadInitializationStatus::AlreadyInitialized;
        }

        const std::string threadName = "thread" + std::to_string(GetThreadID());

        (void)_taskExited->Reset();
        (void)_taskStartGate->Reset();
        _taskExitFinalizationStarted.store(false, std::memory_order_release);

        Task::TaskConfiguration configuration;
        configuration.Name = threadName.c_str();
        configuration.StackSize = GetStackSize();
        configuration.Priority = GetPriority();
        configuration.Core = GetCoreID();

        const auto creation = Task::TaskRuntime::Create(_taskEntry, this, configuration);
        if (!creation) return ThreadInitializationStatus::TaskCreationFailed;

        const auto createdTask = creation.Handle;
        auto expected = System::Execution::InvalidExecutionHandle;
        if (!_taskHandle.compare_exchange_strong(
                expected,
                createdTask,
                std::memory_order_release,
                std::memory_order_acquire
            )) {
            Task::TaskRuntime::Delete(createdTask);
            return ThreadInitializationStatus::ConcurrentInitializationLost;
        }

        configurationLock.unlock();

        struct InitializationContextGuard {
            std::atomic<Task::TaskHandle>& taskHandle;
            std::atomic<bool>& inProgress;

            ~InitializationContextGuard() {
                inProgress.store(false, std::memory_order_release);
                taskHandle.store(
                    System::Execution::InvalidExecutionHandle,
                    std::memory_order_release
                );
            }
        };

        _initializingTaskHandle.store(Task::TaskRuntime::Current(), std::memory_order_release);
        _initializationInProgress.store(true, std::memory_order_release);
        InitializationContextGuard initializationContext{
            _initializingTaskHandle,
            _initializationInProgress
        };

        try {
            OnInitialization();

            const ThreadState stateAfterInitialization = GetThreadState();
            if (
                stateAfterInitialization == ThreadState::Terminating ||
                stateAfterInitialization == ThreadState::Terminated
            ) {
                if (stateAfterInitialization == ThreadState::Terminating) {
                    SetThreadState(ThreadState::Terminated);
                }
                _deleteTask();
                _finalizeStoppedTaskExit();
                return ThreadInitializationStatus::TerminatedDuringInitialization;
            }

            SetThreadState(ThreadState::Initialized);

            const ThreadState stateAfterInitialized = GetThreadState();
            if (
                stateAfterInitialized == ThreadState::Terminating ||
                stateAfterInitialized == ThreadState::Terminated
            ) {
                if (stateAfterInitialized == ThreadState::Terminating) {
                    SetThreadState(ThreadState::Terminated);
                }
                _deleteTask();
                _finalizeStoppedTaskExit();
                return ThreadInitializationStatus::TerminatedDuringInitialization;
            }

            if (
                stateAfterInitialized == ThreadState::Initialized &&
                GetStartOnInitialize()
            ) {
                SetThreadState(ThreadState::Running);
            }
        } catch (...) {
            try { Terminate(); } catch (...) {}
            try { SetThreadState(ThreadState::Terminated); } catch (...) {}
            _deleteTask();
            _finalizeStoppedTaskExit();
            return ThreadInitializationStatus::InitializationException;
        }

        if (!_taskStartGate->Give()) {
            _deleteTask();
            _finalizeStoppedTaskExit();
            return ThreadInitializationStatus::TaskCreationFailed;
        }
        return ThreadInitializationStatus::Success;
    }

public:
    ThreadInitializationStatus Initialize() override {
        const ThreadInitializationStatus status = _initialize();
        if (status != ThreadInitializationStatus::Success) {
            StableCallback<TOnThreadInitializationFailedEvent> callback;
            {
                std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex);
                callback = _onInitializationFailed;
            }
            if (callback != nullptr) {
                try { (*callback)(this, status); } catch (...) {}
            }
            try { _lifecycleObservable->NotifyInitializationFailed(this, status); } catch (...) {}
        }
        return status;
    }

    void Terminate() override {
        switch (GetThreadState()) {
            case ThreadState::Uninitialized:
                SetThreadState(ThreadState::Terminating);
                SetThreadState(ThreadState::Terminated);
                return;
            case ThreadState::Initialized:
            case ThreadState::Running:
            case ThreadState::Paused:
                SetThreadState(ThreadState::Terminating);
                return;
            default:
                return;
        }
    }

    ThreadInitializationStatus Start() override {
        switch (GetThreadState()) {
            case ThreadState::Uninitialized:
            case ThreadState::Terminated: {
                const auto status = Initialize();
                if (status != ThreadInitializationStatus::Success) return status;
                TrySetThreadState(ThreadState::Initialized, ThreadState::Running);
                return status;
            }
            case ThreadState::Initialized:
                TrySetThreadState(ThreadState::Initialized, ThreadState::Running);
                return ThreadInitializationStatus::AlreadyInitialized;
            case ThreadState::Paused:
                TrySetThreadState(ThreadState::Paused, ThreadState::Running);
                return ThreadInitializationStatus::AlreadyInitialized;
            case ThreadState::Running:
                return ThreadInitializationStatus::AlreadyInitialized;
            case ThreadState::Terminating:
            case ThreadState::Destroyed:
                return ThreadInitializationStatus::InvalidState;
        }
        return ThreadInitializationStatus::InvalidState;
    }

    void Pause() override {
        TrySetThreadState(ThreadState::Running, ThreadState::Paused);
    }

    bool TryClaimAutomaticCleanup() override {
        if (!GetFreeOnTerminate() || GetThreadState() != ThreadState::Terminated) return false;
        CleanupClaim expected = CleanupClaim::Available;
        return _cleanupClaim.compare_exchange_strong(
            expected,
            CleanupClaim::Automatic,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    int GetCoreID() override { return _coreID.load(std::memory_order_acquire); }
    uint32_t GetStackSize() override { return _stackSize.load(std::memory_order_acquire); }
    unsigned int GetPriority() override { return _priority.load(std::memory_order_acquire); }
    uint8_t GetThreadID() override { return _threadID; }
    ThreadState GetThreadState() override { return _threadState.load(std::memory_order_acquire); }
    bool GetFreeOnTerminate() override { return _freeOnTerminate.load(std::memory_order_acquire); }
    bool GetStartOnInitialize() override { return _startOnInitialize.load(std::memory_order_acquire); }

    TOnThreadEvent GetOnDestroy() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onDestroy ? *_onDestroy : TOnThreadEvent{}; }
    TOnThreadEvent GetOnInitialize() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onInitialize ? *_onInitialize : TOnThreadEvent{}; }
    TOnThreadEvent GetOnStart() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onStart ? *_onStart : TOnThreadEvent{}; }
    TOnThreadEvent GetOnPause() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onPause ? *_onPause : TOnThreadEvent{}; }
    TOnThreadEvent GetOnTerminate() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onTerminate ? *_onTerminate : TOnThreadEvent{}; }
    TOnThreadEvent GetOnTerminated() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onTerminated ? *_onTerminated : TOnThreadEvent{}; }
    TOnThreadInitializationFailedEvent GetOnInitializationFailed() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onInitializationFailed ? *_onInitializationFailed : TOnThreadInitializationFailedEvent{}; }
    TOnThreadExecutionFailedEvent GetOnExecutionFailed() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onExecutionFailed ? *_onExecutionFailed : TOnThreadExecutionFailedEvent{}; }
    TOnThreadStateChangeEvent GetOnStateChange() override { std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); return _onStateChange ? *_onStateChange : TOnThreadStateChangeEvent{}; }

    void SetCoreID(int value) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_taskConfigurationMutex);
        if (_taskHandle.load(std::memory_order_acquire) == System::Execution::InvalidExecutionHandle) {
            _coreID.store(value, std::memory_order_release);
        }
    }

    void SetStackSize(uint32_t value) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_taskConfigurationMutex);
        if (
            _taskHandle.load(std::memory_order_acquire) == System::Execution::InvalidExecutionHandle &&
            value > 0
        ) _stackSize.store(value, std::memory_order_release);
    }

    void SetPriority(unsigned int value) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_taskConfigurationMutex);
        if (_taskHandle.load(std::memory_order_acquire) == System::Execution::InvalidExecutionHandle) {
            _priority.store(value, std::memory_order_release);
        }
    }

    void SetFreeOnTerminate(bool value) override {
        _freeOnTerminate.store(value, std::memory_order_release);
        if (value) {
            CleanupClaim expected = CleanupClaim::Manual;
            _cleanupClaim.compare_exchange_strong(
                expected,
                CleanupClaim::Available,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );
        } else {
            CleanupClaim expected = CleanupClaim::Available;
            _cleanupClaim.compare_exchange_strong(
                expected,
                CleanupClaim::Manual,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );
        }
    }

    void SetStartOnInitialize(bool value) override {
        _startOnInitialize.store(value, std::memory_order_release);
    }

    void SetOnDestroy(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onDestroy=std::move(c); }
    void SetOnInitialize(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onInitialize=std::move(c); }
    void SetOnStart(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onStart=std::move(c); }
    void SetOnPause(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onPause=std::move(c); }
    void SetOnTerminate(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onTerminate=std::move(c); }
    void SetOnTerminated(TOnThreadEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onTerminated=std::move(c); }
    void SetOnInitializationFailed(TOnThreadInitializationFailedEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onInitializationFailed=std::move(c); }
    void SetOnExecutionFailed(TOnThreadExecutionFailedEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onExecutionFailed=std::move(c); }
    void SetOnStateChange(TOnThreadStateChangeEvent value) override { auto c=MakeStableCallback(std::move(value)); std::lock_guard<System::Synchronization::Mutex> lock(_callbackMutex); _onStateChange=std::move(c); }
};

}
}
