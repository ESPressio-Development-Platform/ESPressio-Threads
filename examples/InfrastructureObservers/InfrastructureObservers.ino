#include <Arduino.h>

#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>
#include <ESPressio_ThreadTerminationDispatcher.hpp>

#include <ESPressio_IThreadManagerObserver.hpp>
#include <ESPressio_IThreadTerminationDispatcherObserver.hpp>

using namespace ESPressio;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Observable::IObserver) + 4 bytes vptr + sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(Observable::IObserver) + 4 bytes vptr + sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
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
 * Inherited Memory Total: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _iterations (uint8_t): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: sizeof(IThread) + 18 bytes known members + sizeof(System::Synchronization::Mutex) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::Mutex) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadEvent>) + sizeof(StableCallback<TOnThreadInitializationFailedEvent>) + sizeof(StableCallback<TOnThreadExecutionFailedEvent>) + sizeof(StableCallback<TOnThreadStateChangeEvent>) + 4 bytes vptr + 1 bytes known members [Thread: _taskExited: owned object: sizeof(System::Synchronization::ISignal); Thread: _taskStartGate: owned object: sizeof(System::Synchronization::ISignal); Thread: _lifecycleObservable: shared control block (~12+ bytes) and, when owning separately, object sizeof(Observable::ThreadSafeObservable)]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
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
