/*
    An extremely simple example of an ESPressio Thread loop.
*/

#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 1600

#include <Arduino.h>
#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio::Threads;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 192 bytes [Thread: _taskExited: owned object: 4 bytes; Thread: _taskStartGate: owned object: 4 bytes; Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _callbackMutex: _owned: owned object: 4 bytes; Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _counter (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 196 bytes [Thread: _taskExited: owned object: 4 bytes; Thread: _taskStartGate: owned object: 4 bytes; Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _callbackMutex: _owned: owned object: 4 bytes; Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class DemoThread : public Thread {
    private:
        uint32_t _counter = 0;

    protected:
        void OnLoop() override {
            Serial.printf(
                "DemoThread: %u\n",
                _counter++
            );

            delay(1000);
        }
};

DemoThread thread;

void setup() {
    Serial.begin(115200);

    thread.SetStartOnInitialize(
        true
    );

    ThreadManager::GetInstance()->
        Initialize();
}

void loop() {
}
