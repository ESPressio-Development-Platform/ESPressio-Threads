#include <Arduino.h>

#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>
#include <ESPressio_ThreadTerminationDispatcher.hpp>

#include <ESPressio_IThreadManagerObserver.hpp>
#include <ESPressio_IThreadTerminationDispatcherObserver.hpp>

using namespace ESPressio;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 8 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class InfrastructureObserver final :
    public Threads::IThreadManagerObserver,
    public Threads::IThreadTerminationDispatcherObserver {

public:
    void OnThreadRegistered(
        Threads::IThread*,
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Thread registered: id=%u core=%d\n",
            snapshot.ThreadID,
            snapshot.CoreID
        );
    }

    void OnThreadRemoved(
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Thread removed: id=%u state=%u\n",
            snapshot.ThreadID,
            static_cast<unsigned int>(
                snapshot.State
            )
        );
    }

    void OnThreadCleanupCompleted(
        const Threads::ThreadManagerCleanupResult& result
    ) override {
        Serial.printf(
            "Manager cleanup: examined=%u claimed=%u removed=%u deleted=%u deferred=%u\n",
            static_cast<unsigned int>(result.ThreadsExamined),
            static_cast<unsigned int>(result.ThreadsClaimed),
            static_cast<unsigned int>(result.ThreadsRemoved),
            static_cast<unsigned int>(result.ThreadsDeleted),
            result.WasDeferred ? 1U : 0U
        );
    }

    void OnThreadTerminationDispatchQueued(
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Termination dispatch queued: id=%u\n",
            snapshot.ThreadID
        );
    }
};


/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 192 bytes [Thread: _taskExited: owned object: 4 bytes; Thread: _taskStartGate: owned object: 4 bytes; Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _callbackMutex: _owned: owned object: 4 bytes; Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _iterations (uint8_t): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 196 bytes [Thread: _taskExited: owned object: 4 bytes; Thread: _taskStartGate: owned object: 4 bytes; Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _callbackMutex: _owned: owned object: 4 bytes; Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class DemoThread final :
    public Threads::Thread {

private:
    uint8_t _iterations = 0;

protected:
    void OnLoop() override {
        Serial.printf(
            "Demo iteration %u\n",
            ++_iterations
        );

        if (_iterations == 3) {
            Terminate();
        }

        delay(250);
    }
};


InfrastructureObserver infrastructureObserver;

Observable::ObserverHandlePtr
    managerObserverHandle;

Observable::ObserverHandlePtr
    terminationDispatcherObserverHandle;

DemoThread* demoThread = nullptr;


void setup() {
    Serial.begin(115200);

    managerObserverHandle =
        Threads::ThreadManager::
            GetInstance()->
            RegisterObserver(
                &infrastructureObserver
            );

    terminationDispatcherObserverHandle =
        Threads::ThreadTerminationDispatcher::
            GetInstance()->
            RegisterObserver(
                &infrastructureObserver
            );

    /*
     * Construct after observer registration so manager registration, normal
     * termination dispatch, and manager-owned ReleaseOnTerminate reclamation
     * are all visible in this example.
     */
    demoThread =
        new DemoThread();

    demoThread->SetFreeOnTerminate(
        true
    );

    demoThread->SetStartOnInitialize(
        true
    );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}


void loop() {
    delay(1000);
}
