#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _count (uint8_t): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr + 1 bytes known members [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class CountingThread final :
    public Threads::Thread {

    private:
        uint8_t _count = 0;

    protected:
        void OnLoop() override {
            Serial.printf(
                "iteration %u\n",
                ++_count
            );

            if (_count == 3) {
                Terminate();
            }

            vTaskDelay(
                pdMS_TO_TICKS(250)
            );
        }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadLifecycleLogger final :
    public Threads::IThreadObserver {

    public:
        void OnThreadStateChanged(
            Threads::IThread* thread,
            Threads::ThreadState oldState,
            Threads::ThreadState newState
        ) override {
            Serial.printf(
                "thread %u state %u -> %u\n",
                thread->GetThreadID(),
                static_cast<unsigned int>(
                    oldState
                ),
                static_cast<unsigned int>(
                    newState
                )
            );
        }

        void OnThreadInitializationFailed(
            Threads::IThread* thread,
            Threads::ThreadInitializationStatus status
        ) override {
            Serial.printf(
                "thread %u initialization failed: %u\n",
                thread->GetThreadID(),
                static_cast<unsigned int>(
                    status
                )
            );
        }

        void OnThreadTaskExited(
            Threads::IThread* thread
        ) override {
            Serial.printf(
                "thread %u FreeRTOS task exited\n",
                thread->GetThreadID()
            );
        }
};

ThreadLifecycleLogger lifecycleLogger;
CountingThread countingThread;

Observable::ObserverHandlePtr
    lifecycleObserverHandle;

void setup() {
    Serial.begin(115200);

    lifecycleObserverHandle =
        countingThread.
            RegisterThreadObserver(
                &lifecycleLogger
            );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}

void loop() {
    delay(1000);
}
