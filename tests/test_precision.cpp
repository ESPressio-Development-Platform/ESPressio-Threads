#include "TestRuntime.hpp"
#include <ESPressio_Precision.hpp>
#include <cassert>
#include <cmath>
using namespace ESPressio::Threads;
int main() {
    HostRuntime runtime;
    ManualClock clock;
    ThreadHostServices host{&clock,[](void*,bool) noexcept { return true; },[](const void*) noexcept { return true; },
        [](const void* p) noexcept { return static_cast<const ManualClock*>(p)->NowNanoseconds(); }};
    Precision<4> precision;
    precision.SetCadencePeriod(100);
    assert(precision.Initialize(host)==ThreadStatus::Success);
    assert(precision.FinalizeInitialization()==ThreadStatus::Success);
    ThreadCycleContext context{100,host}; clock.Value=100; precision.Activate(context);
    assert(precision.Readiness(context).ApplicationDeadlineDue);
    assert(precision.BeforeApplication(context));
    assert(precision.CurrentIteration().Delta==0 && precision.CurrentIteration().SkippedIterationCount==0);
    precision.SetIterationExecutionBudget(50);
    assert(precision.RemainingExecutionBudget(130)==20 && precision.RemainingExecutionBudget(160)==-10);
    context.Now=120; precision.AfterApplication(context);
    assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==200 && !precision.Readiness(context).ApplicationEligible);
    context.Now=450; assert(precision.BeforeApplication(context));
    auto iteration=precision.CurrentIteration();
    assert(iteration.ScheduledStartTime==200 && iteration.Lateness==250 && iteration.SkippedIterationCount==2 && iteration.Delta==350);
    context.Now=470; precision.AfterApplication(context);
    assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==500);
    assert(precision.SetSampleWindow(2) && !precision.SetSampleWindow(5));
    assert(precision.SetDeltaMode(PrecisionDeltaMode::EndToStart));
    for (std::uint64_t start: {500,600,700,800}) {
        context.Now=start; assert(precision.BeforeApplication(context));
        assert(precision.CurrentIteration().Delta==(start==500 ? 0 : 80));
        context.Now=start+20; precision.AfterApplication(context);
    }
    auto metrics=precision.GetTelemetry();
    assert(metrics.RetainedSamples==2 && metrics.AverageFrequencyHertz==10000000 && metrics.CurrentFrequencyHertz==10000000);
    clock.Value=850; precision.SetCadencePeriod(200); context.Now=850;
    assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==1050 && !precision.Readiness(context).ApplicationEligible);
    // An earlier Inspect does not authorize an application iteration after a concurrent tuning change.
    assert(!precision.BeforeApplication(context));
    clock.Value=900; precision.Bump(); context.Now=900; assert(precision.BeforeApplication(context));
    context.Now=910; precision.AfterApplication(context);
    assert(precision.Readiness(context).Deadline.MonotonicNanoseconds==1100);
    precision.Pause(context); clock.Value=10000; context.Now=10000; precision.Activate(context);
    assert(precision.BeforeApplication(context));
    assert(precision.CurrentIteration().Delta==0 && precision.CurrentIteration().SkippedIterationCount==0);
    precision.AfterApplication(context);
    precision.SetCadencePeriod(0); assert(!precision.Readiness(context).Deadline.Present);
    assert(precision.Readiness(context).ApplicationEligible);
    precision.Bump(); assert(precision.Readiness(context).ApplicationDeadlineDue);
    assert(precision.BeforeApplication(context)); precision.AfterApplication(context);
    assert(!precision.Readiness(context).ApplicationDeadlineDue);
    precision.SetSampleWindow(0); context.Now+=100; assert(precision.BeforeApplication(context)); precision.AfterApplication(context);
    context.Now+=100; assert(precision.BeforeApplication(context)); precision.AfterApplication(context);
    assert(precision.GetTelemetry().RetainedSamples==0 && precision.GetTelemetry().CurrentFrequencyHertz==10000000);
    clock.Value=UINT64_MAX; context.Now=UINT64_MAX;
    precision.SetCadencePeriod(100);
    assert(precision.GetTelemetry().CadenceExhausted && !precision.Readiness(context).Deadline.Present);
    precision.Bump(); assert(precision.BeforeApplication(context)); precision.AfterApplication(context);
    assert(precision.GetTelemetry().CadenceExhausted && !precision.Readiness(context).ApplicationEligible);
    precision.Quiesce(context); assert(!precision.Readiness(context).ApplicationEligible);
}
