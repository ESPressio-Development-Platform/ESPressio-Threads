#include <Arduino.h>
#include <ESPressio_ThreadWith.hpp>
#include <ESPressio_Precision.hpp>
#include <ESPressio_ThreadStackTelemetry.hpp>
using namespace ESPressio::Threads;
class Instrument final : public ThreadWith<Precision<4>> {
protected:
    ThreadWorkDisposition OnLoop() override { return ThreadWorkDisposition::IdleReady; }
public:
    Instrument() { GetCapability<PrecisionTag>().SetCadencePeriod(100000000); }
    ~Instrument() override { (void)Shutdown(); }
} instrument;
void setup() {
    // Install platform providers first; compile-only examples make no hardware certification claim.
    const auto profile=GetThreadResourceProfile(instrument);
    (void)profile;
    if (instrument.Initialize()==ThreadStatus::Success) (void)instrument.Start();
}
void loop() {
    const auto diagnostics=instrument.GetDiagnostics();
    const auto stack=GetThreadStackTelemetry(instrument);
    (void)diagnostics; (void)stack;
    yield();
}
