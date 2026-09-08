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
 * Inherited Memory Total: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _counter (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr + 4 bytes known members [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
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
