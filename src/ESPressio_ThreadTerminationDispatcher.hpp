#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

#include <ESPressio_IObservable.hpp>
#include <ESPressio_Queue.hpp>
#include <ESPressio_Synchronization.hpp>
#include <ESPressio_Task.hpp>

#include "ESPressio_IThreadTerminationDispatcherObserver.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE 2000
#endif

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY 2
#endif

#ifndef ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH
    #define ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH 32
#endif

namespace ESPressio {
namespace Threads {

class Thread;

/// <summary>Singleton task that performs thread termination outside the terminating thread's own execution context.</summary>
/// <remarks>Termination requests are queued and processed by a dedicated Task-backed dispatcher. Initialization/resource publication is serialized through ESPressio System synchronization.</remarks>
class ThreadTerminationDispatcher {
private:
    class DispatcherObservable final : public Observable::ThreadSafeObservable {
    private:
        template <typename TCallback>
        void NotifyObservers(TCallback callback) {
            ExecuteNotification([&](NotificationContext& notification) {
                notification.WithObservers<IThreadTerminationDispatcherObserver>(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        try { callback(observer); } catch (...) {}
                    }
                );
            });
        }

    public:
        void Initialized(bool available) {
            NotifyObservers([&](IThreadTerminationDispatcherObserver* observer) {
                observer->OnThreadTerminationDispatcherInitialized(available);
            });
        }
        void Queued(const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadTerminationDispatcherObserver* observer) {
                observer->OnThreadTerminationDispatchQueued(snapshot);
            });
        }
        void QueueFailed(const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadTerminationDispatcherObserver* observer) {
                observer->OnThreadTerminationDispatchQueueFailed(snapshot);
            });
        }
        void Started(const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadTerminationDispatcherObserver* observer) {
                observer->OnThreadTerminationDispatchStarted(snapshot);
            });
        }
        void Completed(const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadTerminationDispatcherObserver* observer) {
                observer->OnThreadTerminationDispatchCompleted(snapshot);
            });
        }
    };

    struct DispatchRecord {
        Thread* ThreadPointer = nullptr;
        ThreadManagerThreadSnapshot Snapshot;
    };

    std::unique_ptr<System::Queue::IMessageQueue> _queue;
    Task::TaskHandle _taskHandle = System::Execution::InvalidExecutionHandle;
    mutable System::Synchronization::Mutex _initializationMutex;

    std::shared_ptr<DispatcherObservable> _observable =
        std::make_shared<DispatcherObservable>();

    ThreadTerminationDispatcher() = default;

    bool _initialize();
    static void _taskEntry(void* parameter);
    void _loop();

public:
    ThreadTerminationDispatcher(const ThreadTerminationDispatcher&) = delete;
    ThreadTerminationDispatcher& operator=(const ThreadTerminationDispatcher&) = delete;

    /// <summary>Returns the process-wide termination dispatcher singleton.</summary>
    static ThreadTerminationDispatcher* GetInstance();
    /// <summary>Indicates whether the dispatcher task and queue are currently available.</summary>
    bool IsAvailable() const;
    /// <summary>Ensures dispatcher resources are initialized and reports whether they are available.</summary>
    bool EnsureAvailable();
    /// <summary>Indicates whether the caller is currently executing on the dispatcher task.</summary>
    bool IsCurrentTask() const;
    /// <summary>Returns the minimum free stack bytes observed for the dispatcher task, or zero when unavailable.</summary>
    uint32_t GetMinimumFreeStackBytes() const;
    /// <summary>Queues a thread for asynchronous termination processing.</summary>
    bool Dispatch(Thread* thread);

    /// <summary>Registers an observer for dispatcher availability and dispatch lifecycle notifications.</summary>
    Observable::ObserverHandlePtr RegisterObserver(
        IThreadTerminationDispatcherObserver* observer
    ) {
        auto handle = _observable->RegisterObserver(observer);
        if (observer != nullptr && IsAvailable()) {
            try { observer->OnThreadTerminationDispatcherInitialized(true); } catch (...) {}
        }
        return handle;
    }

    /// <summary>Unregisters a termination-dispatcher observer.</summary>
    void UnregisterObserver(IThreadTerminationDispatcherObserver* observer) {
        _observable->UnregisterObserver(observer);
    }
};

}
}
