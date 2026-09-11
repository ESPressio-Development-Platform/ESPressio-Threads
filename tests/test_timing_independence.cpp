#include "TestRuntime.hpp"
#include <ESPressio_Precision.hpp>
#include <ESPressio_Thread.hpp>
#include <ESPressio_TimingSystemClock.hpp>
#include <ClockTestEvidence.hpp>
#include <cassert>
using namespace ESPressio;
using namespace ESPressio::Threads;
int main() {
    HostRuntime runtime; ManualClock monotonic;
    System::Clock::SetMonotonicClock(&monotonic);
    static_assert(!std::is_constructible_v<Thread,ThreadConfiguration,Timing::SystemClock<>*>,
        "A synchronized/rebasable System clock cannot be injected into Thread scheduling");
    ThreadHostServices host{&monotonic,[](void*,bool) noexcept { return true; },[](const void*) noexcept { return true; },
        [](const void* p) noexcept { return static_cast<const ManualClock*>(p)->NowNanoseconds(); }};
    Precision<4> precision; precision.SetCadencePeriod(100000000);
    assert(precision.Initialize(host)==ThreadStatus::Success);
    ThreadCycleContext context{0,host}; precision.Activate(context);
    assert(precision.BeforeApplication(context)); precision.AfterApplication(context);
    auto& system=Timing::SystemClock<Timing::DefaultClockTime,Timing::NoLockPolicy>::GetInstance();
    const auto deadline=precision.Readiness(context).Deadline.MonotonicNanoseconds;
    assert(system.TrySetTime(Timing::DefaultClockTime(999,Units::Base))==Timing::ClockConfigurationStatus::Success);
    assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==deadline);
    assert(system.TrySetTime(Timing::DefaultClockTime(0,Units::Nano))==Timing::ClockConfigurationStatus::Success);
    assert(system.SelectSynchronizationReference(1)==Timing::ClockConfigurationStatus::Success);
    system.SealContinuity();
    for (unsigned i=0;i<5;++i) {
        const auto start=1000000000ull+i*250000000ull;
        monotonic.Value=start;
        const auto model=system.GetClockModelSnapshot();
        const auto observation=ClockTest::Observation(start,10000,10000,1,100,&model);
        monotonic.Value=start+10000;
        context.Now=monotonic.Value;
        const auto nominal=precision.Readiness(context).Deadline.MonotonicNanoseconds;
        const auto before=system.CaptureQualifiedTime().Nanoseconds;
        assert(system.SubmitSynchronizationObservation(observation).Accepted);
        assert(system.CaptureQualifiedTime().Nanoseconds==before);
        assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==nominal);
        assert(precision.BeforeApplication(context));
        assert(precision.CurrentIteration().ScheduledStartTime==nominal);
        precision.AfterApplication(context);
    }
    assert(system.GetSynchronizationStatus().Reliability==Timing::TimeReliability::Synchronized);
    System::Clock::ResetMonotonicClock();
}
