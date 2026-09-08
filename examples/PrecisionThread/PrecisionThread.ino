#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

constexpr uint8_t HeartbeatPin = 2;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 356 bytes [PrecisionThread: Thread: _taskExited: owned object: 4 bytes; PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _scheduleSignal: owned object: 4 bytes; PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _ledState (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 360 bytes [PrecisionThread: Thread: _taskExited: owned object: 4 bytes; PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _scheduleSignal: owned object: 4 bytes; PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class HeartbeatThread final :
    public Threads::PrecisionThread<> {

    private:
        bool _ledState = false;

    protected:
        void Iterate(
            IterationTime delta,
            IterationTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            (void)delta;
            (void)startTime;
            (void)skippedIterations;

            _ledState = !_ledState;

            digitalWrite(
                HeartbeatPin,
                _ledState ? HIGH : LOW
            );
        }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class HeartbeatObserver final :
    public Threads::IPrecisionThreadObserver<> {

    public:
        void OnPrecisionThreadIteration(
            Threads::PrecisionThread<>* thread,
            Timing::DefaultClockTime delta,
            Timing::DefaultClockTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            (void)thread;

            Serial.printf(
                "iteration at %llu ms; delta=%llu us; skipped=%llu\n",
                static_cast<unsigned long long>(
                    startTime.ToMagnitude<uint64_t>(
                        Units::Milli
                    )
                ),
                static_cast<unsigned long long>(
                    delta.ToMagnitude<uint64_t>(
                        Units::Micro
                    )
                ),
                static_cast<unsigned long long>(
                    skippedIterations
                )
            );
        }
};

HeartbeatObserver heartbeatObserver;
HeartbeatThread heartbeat;

Observable::ObserverHandlePtr
    heartbeatObserverHandle;

void setup() {
    Serial.begin(115200);
    pinMode(HeartbeatPin, OUTPUT);

    heartbeat.SetIterationPeriod(
        Units::MilliSeconds<uint64_t>(
            500
        )
    );

    heartbeat.SetIterationDeltaMode(
        Threads::IterationDeltaMode::
            StartToStart
    );

    heartbeat.SetIterationSampleCount(
        10
    );

    heartbeatObserverHandle =
        heartbeat.RegisterIterationObserver(
            &heartbeatObserver
        );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}

void loop() {
    const double currentFrequency =
        heartbeat.
            GetIterationFrequency().
            value;

    const double averageFrequency =
        heartbeat.
            GetAverageIterationFrequency().
            value;

    (void)currentFrequency;
    (void)averageFrequency;

    delay(1000);
}
