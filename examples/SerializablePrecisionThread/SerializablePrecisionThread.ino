#include <ESPressio_PrecisionThread_Serializable.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

using SerializableThreadTime =
    Units::SerializableNanoSeconds<
        uint64_t
    >;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Thread) + 127 bytes known members + sizeof(System::Synchronization::Mutex) + 4 bytes vptr [PrecisionThread: _scheduleSignal: owned object: sizeof(System::Synchronization::ISignal); PrecisionThread: _iterationSamples: implementation blocks containing N * 8 bytes plus map pointers]
 * Requires Stack/Heap Preallocation
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(Thread) + 127 bytes known members + sizeof(System::Synchronization::Mutex) + 4 bytes vptr [PrecisionThread: _scheduleSignal: owned object: sizeof(System::Synchronization::ISignal); PrecisionThread: _iterationSamples: implementation blocks containing N * 8 bytes plus map pointers]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class SerializableWorker final :
    public Threads::PrecisionThread<
        SerializableThreadTime
    > {

    protected:
        void Iterate(
            IterationTime delta,
            IterationTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            /*
             * With the optional Serializable policy header:
             *
             *   IterationTime
             *   SignedIterationTime
             *   IterationFrequency
             *
             * are all Serializable Unit types.
             */
            (void)delta;
            (void)startTime;
            (void)skippedIterations;
        }
};

SerializableWorker worker;

void setup() {
    Serial.begin(115200);

    worker.SetIterationPeriod(
        Units::MilliSeconds<
            uint64_t
        >(500)
    );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}

void loop() {
    auto available =
        worker.
            GetAvailableIterationTime();

    auto frequency =
        worker.
            GetAverageIterationFrequency();

    (void)available;
    (void)frequency;

    delay(1000);
}
