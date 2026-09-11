#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <ESPressio_Synchronization.hpp>
#include <ESPressio_ClockUncertainty.hpp>
#include "ESPressio_ThreadCapability.hpp"
namespace ESPressio::Threads {
struct PrecisionTag final {};
enum class PrecisionDeltaMode : std::uint8_t { StartToStart, EndToStart };
/// <summary>Read-only during the owning Thread's OnLoop; every coordinate is raw monotonic nanoseconds.</summary>
struct PrecisionIterationContext final {
    std::uint64_t ScheduledStartTime=0,StartTime=0,Lateness=0,Delta=0,SkippedIterationCount=0;
};
struct PrecisionTelemetry final {
    PrecisionIterationContext Iteration{};
    double CurrentFrequencyHertz=0,AverageFrequencyHertz=0;
    std::uint64_t CompletedIterations=0,TotalSkippedIterations=0,LastExecutionNanoseconds=0;
    std::size_t RetainedSamples=0,ActiveSampleWindow=0;
    bool IterationActive=false,CadenceExhausted=false;
};
/// <summary>Monotonic cadence and fixed O(1) telemetry over the root loop/common Wake; never a second worker.</summary>
template<std::size_t MaximumIterationSamples=8> class Precision final : public ThreadCapability {
    static_assert(MaximumIterationSamples>0,"Precision requires a positive fixed telemetry capacity");
    using Mutex=System::Synchronization::Mutex;
    mutable Mutex _mutex;
    ThreadHostServices _host{};
    std::atomic<std::uint64_t> _period{0},_deadline{0};
    std::atomic<bool> _active{false},_forced{false},_exhausted{false};
    PrecisionDeltaMode _deltaMode=PrecisionDeltaMode::StartToStart;
    std::uint64_t _budget=0,_previousStart=0,_previousEnd=0;
    bool _hasPrevious=false,_iterationActive=false,_measurementsReset=false;
    PrecisionIterationContext _iteration{};
    std::array<std::uint64_t,MaximumIterationSamples> _samples{};
    std::size_t _window=MaximumIterationSamples,_count=0,_next=0;
    long double _sum=0;
    double _frequency=0;
    std::uint64_t _completed=0,_skipped=0,_lastExecution=0;
    void ResetMeasurements() noexcept {
        _samples={}; _count=0; _next=0; _sum=0; _frequency=0; _hasPrevious=false; _measurementsReset=true;
    }
    void Sample(std::uint64_t delta) noexcept {
        if (!delta) return;
        _frequency=1000000000.0/static_cast<double>(delta);
        if (!_window) return;
        if (_count==_window) _sum-=static_cast<long double>(_samples[_next]); else ++_count;
        _samples[_next]=delta; _sum+=static_cast<long double>(delta); _next=(_next+1)%_window;
    }
public:
    using CapabilityTag=PrecisionTag;
    using ExclusiveClaims=CapabilityClaims<ApplicationCadenceRole>;
    static constexpr bool NeedsMonotonicTime=true;
    static constexpr std::uint32_t FrameworkStackFloorBytes=1024;
    ThreadStatus Initialize(const ThreadHostServices& host) {
        std::lock_guard<Mutex> lock(_mutex); _host=host; _active.store(false); ResetMeasurements();
        return ThreadStatus::Success;
    }
    void RollbackInitialization() noexcept { std::lock_guard<Mutex> lock(_mutex); _host={}; _active.store(false); }
    void Activate(const ThreadCycleContext& context) {
        std::lock_guard<Mutex> lock(_mutex); ResetMeasurements(); _iterationActive=false; _forced.store(false,std::memory_order_relaxed); _exhausted.store(false,std::memory_order_relaxed);
        _deadline.store(context.Now,std::memory_order_relaxed); _active.store(true,std::memory_order_release);
    }
    void Pause(const ThreadCycleContext&) { _active.store(false,std::memory_order_release); }
    void Quiesce(const ThreadCycleContext&) noexcept { _active.store(false,std::memory_order_release); }
    /// <summary>The negative path reads only scalar readiness; no clock read, mutex, sample traversal or callback.</summary>
    CapabilityReadiness Readiness(const ThreadCycleContext& context) const noexcept {
        if (!_active.load(std::memory_order_acquire) || _exhausted.load(std::memory_order_acquire)) return {false,{},false,false};
        const auto period=_period.load(std::memory_order_acquire);
        if (!period) return {false,{},true,_forced.load(std::memory_order_acquire)};
        const auto deadline=_deadline.load(std::memory_order_acquire);
        return {false,ThreadDeadline::At(deadline),context.Now>=deadline,context.Now>=deadline};
    }
    bool BeforeApplication(const ThreadCycleContext& context) {
        std::lock_guard<Mutex> lock(_mutex);
        const auto now=context.Now,period=_period.load(std::memory_order_relaxed);
        const auto nominal=period ? _deadline.load(std::memory_order_relaxed) : now;
        if (!_active.load(std::memory_order_acquire) || _exhausted.load(std::memory_order_acquire) || (period && now<nominal)) return false;
        _forced.store(false,std::memory_order_release);
        const auto lateness=now>=nominal ? now-nominal : 0;
        const auto skipped=period ? lateness/period : 0;
        const auto baseline=_deltaMode==PrecisionDeltaMode::StartToStart ? _previousStart : _previousEnd;
        _iteration={nominal,now,lateness,_hasPrevious && now>=baseline ? now-baseline : 0,skipped};
        if (_hasPrevious && now>=_previousStart) Sample(now-_previousStart);
        _skipped=Timing::ClockMath::Add(_skipped,skipped);
        if (period) {
            // Advance from nominal to the first future slot, without multiplication overflow or a catch-up burst.
            const auto remainder=lateness%period;
            _exhausted.store(period-remainder>UINT64_MAX-now,std::memory_order_release);
            _deadline.store(Timing::ClockMath::Add(now,period-remainder),std::memory_order_release);
        }
        _iterationActive=true; _measurementsReset=false; return true;
    }
    void AfterApplication(const ThreadCycleContext& context) {
        std::lock_guard<Mutex> lock(_mutex);
        _lastExecution=context.Now>=_iteration.StartTime ? context.Now-_iteration.StartTime : 0;
        _completed=Timing::ClockMath::Add(_completed,1);
        if (!_measurementsReset) { _previousStart=_iteration.StartTime; _previousEnd=context.Now; _hasPrevious=true; }
        _iterationActive=false;
    }
    /// <summary>Scalar runtime tuning; positive changes re-anchor to now + period and never force an early iteration.</summary>
    void SetCadencePeriod(std::uint64_t nanoseconds) {
        { std::lock_guard<Mutex> lock(_mutex);
          const auto now=_host.Now();
          _exhausted.store(nanoseconds>UINT64_MAX-now,std::memory_order_release);
          _deadline.store(Timing::ClockMath::Add(now,nanoseconds),std::memory_order_relaxed);
          _period.store(nanoseconds,std::memory_order_release); }
        (void)_host.Wake();
    }
    std::uint64_t CadencePeriod() const noexcept { return _period.load(std::memory_order_acquire); }
    /// <summary>Force the next eligible slot now; subsequent cadence follows this new anchor.</summary>
    void Bump() {
        { std::lock_guard<Mutex> lock(_mutex); _deadline.store(_host.Now(),std::memory_order_release); _forced.store(true,std::memory_order_release); _exhausted.store(false,std::memory_order_release); }
        (void)_host.Wake();
    }
    bool SetSampleWindow(std::size_t samples) {
        if (samples>MaximumIterationSamples) return false;
        std::lock_guard<Mutex> lock(_mutex); _window=samples; ResetMeasurements(); return true;
    }
    bool SetDeltaMode(PrecisionDeltaMode mode) {
        if (mode!=PrecisionDeltaMode::StartToStart && mode!=PrecisionDeltaMode::EndToStart) return false;
        std::lock_guard<Mutex> lock(_mutex); _deltaMode=mode; ResetMeasurements(); return true;
    }
    void SetIterationExecutionBudget(std::uint64_t nanoseconds) { std::lock_guard<Mutex> lock(_mutex); _budget=nanoseconds; }
    std::int64_t RemainingExecutionBudget(std::uint64_t now) const {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_budget || !_iterationActive) return 0;
        const auto deadline=Timing::ClockMath::Add(_iteration.StartTime,_budget);
        if (deadline>=now) return static_cast<std::int64_t>(std::min<std::uint64_t>(deadline-now,INT64_MAX));
        const auto overrun=now-deadline;
        return overrun>static_cast<std::uint64_t>(INT64_MAX) ? INT64_MIN : -static_cast<std::int64_t>(overrun);
    }
    const PrecisionIterationContext& CurrentIteration() const noexcept { return _iteration; }
    PrecisionTelemetry GetTelemetry() const {
        std::lock_guard<Mutex> lock(_mutex);
        return {_iteration,_frequency,_sum>0 ? static_cast<double>(_count*1000000000.0L/_sum) : 0,
            _completed,_skipped,_lastExecution,_count,_window,_iterationActive,_exhausted.load()};
    }
};
}
