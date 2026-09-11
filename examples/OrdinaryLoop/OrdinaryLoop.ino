#include <Arduino.h>
#include <ESPressio_Thread.hpp>
using namespace ESPressio::Threads;
class Counter final : public Thread {
    unsigned value=0;
protected:
    ThreadWorkDisposition OnLoop() override {
        if (++value==10) Terminate();
        return ThreadWorkDisposition::ImmediateWorkRemaining;
    }
public:
    ~Counter() override { (void)Shutdown(); }
} counter;
void setup() {
    // Install the platform's joinable execution and synchronization providers first.
    if (counter.Initialize()==ThreadStatus::Success) (void)counter.Start();
}
void loop() { yield(); }
