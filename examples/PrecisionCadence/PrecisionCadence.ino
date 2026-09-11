#include <Arduino.h>
#include <ESPressio_ThreadWith.hpp>
#include <ESPressio_Precision.hpp>
using namespace ESPressio::Threads;
class Heartbeat final : public ThreadWith<Precision<8>> {
    bool high=false;
protected:
    ThreadWorkDisposition OnLoop() override {
        const auto& iteration=GetCapability<PrecisionTag>().CurrentIteration();
        (void)iteration;
        high=!high; digitalWrite(2,high ? HIGH : LOW);
        return ThreadWorkDisposition::IdleReady;
    }
public:
    Heartbeat() { GetCapability<PrecisionTag>().SetCadencePeriod(500000000); }
    ~Heartbeat() override { (void)Shutdown(); }
} heartbeat;
void setup() {
    // Install the platform providers before initializing this composition.
    pinMode(2,OUTPUT);
    if (heartbeat.Initialize()==ThreadStatus::Success) (void)heartbeat.Start();
}
void loop() { yield(); }
