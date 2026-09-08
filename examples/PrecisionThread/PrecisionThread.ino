#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

constexpr uint8_t HeartbeatPin = 2;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Thread) + 127 bytes known members + sizeof(System::Synchronization::Mutex) + 4 bytes vptr [PrecisionThread: _scheduleSignal: owned object: sizeof(System::Synchronization::ISignal); PrecisionThread: _iterationSamples: implementation blocks containing N * 8 bytes plus map pointers]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _ledState (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: sizeof(Thread) + 127 bytes known members + sizeof(System::Synchronization::Mutex) + 4 bytes vptr + 1 bytes known members [PrecisionThread: _scheduleSignal: owned object: sizeof(System::Synchronization::ISignal); PrecisionThread: _iterationSamples: implementation blocks containing N * 8 bytes plus map pointers]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
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
 * Inherited Memory Total: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
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
