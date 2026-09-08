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
/**
 * ESPressio Memory Audit
 * Members:
 * - _queue (std::unique_ptr<System::Queue::IMessageQueue>): 4 bytes [owned object: 4 bytes]
 * - _taskHandle (Task::TaskHandle): 4 bytes [0 bytes dynamic allocation]
 * - _initializationMutex (System::Synchronization::Mutex): 20 bytes [_owned: owned object: 4 bytes; _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * - _observable (std::shared_ptr<DispatcherObservable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Total Memory: 36 bytes [_queue: owned object: 4 bytes; _initializationMutex: _owned: owned object: 4 bytes; _initializationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _observable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadTerminationDispatcher {
private:
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Members: none (standalone empty object occupies 1 byte; an eligible empty base may be optimized to 0 bytes).
 * Total Memory: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members:
 * - ThreadPointer (Thread*): 4 bytes [0 bytes dynamic allocation]
 * - Snapshot (ThreadManagerThreadSnapshot): 16 bytes [0 bytes dynamic allocation]
 * Total Memory: 20 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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
