#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>

#include <ESPressio_Synchronization.hpp>

#include "ESPressio_Frequency.hpp"
#include "ESPressio_IPrecisionThreadObserver.hpp"
#include "ESPressio_PrecisionThreadTraits.hpp"
#include "ESPressio_ISystemClock.hpp"
#include "ESPressio_SystemClock.hpp"
#include "ESPressio_TimeTraits.hpp"
#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Selects how PrecisionThread calculates the delta supplied to each iteration.</summary>
    enum class IterationDeltaMode : uint8_t {
        /// <summary>Measures from the previous iteration start to the current iteration start.</summary>
        StartToStart,
        /// <summary>Measures from the previous iteration end to the current iteration start.</summary>
        EndToStart
    };

    /// <summary>Thread base class that executes work against a clock-driven iteration schedule with timing telemetry.</summary>
    /// <typeparam name="TTime">Public time representation used by the scheduler.</typeparam>
    /// <typeparam name="TRepresentationTraits">Traits used to construct frequency and signed-time results.</typeparam>
    template<typename TTime, typename TRepresentationTraits>
    class PrecisionThread : public Thread {
    public:
        using RepresentationTraits = TRepresentationTraits;
        using IterationTime = TTime;
        using TimeType = IterationTime;
        using ClockType = Timing::ISystemClock<IterationTime>;
        using IterationFrequency = typename RepresentationTraits::IterationFrequency;
        using SignedIterationTime = typename RepresentationTraits::SignedIterationTime;

    private:
        class IterationObservable final : public Observable::ThreadSafeObservable {
        public:
            void Notify(
                PrecisionThread<TTime, TRepresentationTraits>* thread,
                IterationTime delta,
                IterationTime startTime,
                SkippedIterationCount skippedIterations
            ) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<
                        IPrecisionThreadObserver<TTime, TRepresentationTraits>
                    >([&](IPrecisionThreadObserver<TTime, TRepresentationTraits>* observer) {
                        try {
                            observer->OnPrecisionThreadIteration(
                                thread,
                                delta,
                                startTime,
                                skippedIterations
                            );
                        } catch (...) {}
                    });
                });
            }
        };

        ClockType* _clock;
        std::shared_ptr<IterationObservable> _iterationObservable =
            std::make_shared<IterationObservable>();
        std::unique_ptr<System::Synchronization::ISignal> _scheduleSignal;
        mutable std::mutex _timingMutex;
        IterationDeltaMode _deltaMode = IterationDeltaMode::StartToStart;
        uint64_t _iterationPeriodNanoseconds = 0;
        uint64_t _desiredIterationPeriodNanoseconds = 0;
        uint32_t _iterationSampleCount = 10;
        std::deque<uint64_t> _iterationSamples;
        double _iterationFrequency = 0.0;
        double _averageIterationFrequency = 0.0;
        bool _scheduleInitialized = false;
        bool _hasPreviousIteration = false;
        uint64_t _previousStartNanoseconds = 0;
        uint64_t _previousEndNanoseconds = 0;
        uint64_t _nextIterationNanoseconds = 0;
        uint64_t _activeIterationStartNanoseconds = 0;
        uint64_t _measurementGeneration = 0;
        std::atomic<bool> _workWakeRequested{false};

        static uint64_t _addSaturated(uint64_t left, uint64_t right) {
            const uint64_t maximum = std::numeric_limits<uint64_t>::max();
            return right > maximum - left ? maximum : left + right;
        }

        static uint64_t _toNanoseconds(const IterationTime& time) {
            return Timing::TimeTraits<IterationTime>::template ToNanoseconds<uint64_t>(time);
        }

        IterationTime _fromNanoseconds(uint64_t nanoseconds) const {
            uint64_t resolution =
                Timing::TimeTraits<IterationTime>::template ToNanoseconds<uint64_t>(
                    _clock->GetResolution()
                );
            if (resolution == 0) resolution = 1;
            return Timing::TimeTraits<IterationTime>::template FromNanoseconds<uint64_t>(
                nanoseconds,
                resolution
            );
        }

        uint64_t _getNowNanoseconds() const {
            return _toNanoseconds(_clock->GetTime());
        }

        void _signalScheduler() {
            if (_scheduleSignal != nullptr) {
                (void)_scheduleSignal->Give();
            }
        }

        void _resetMeasurementsLocked() {
            ++_measurementGeneration;
            _hasPreviousIteration = false;
            _previousStartNanoseconds = 0;
            _previousEndNanoseconds = 0;
            _activeIterationStartNanoseconds = 0;
            _iterationSamples.clear();
            _iterationFrequency = 0.0;
            _averageIterationFrequency = 0.0;
        }

        void _resetSchedule(bool clearMeasurements = true) {
            {
                std::lock_guard<std::mutex> lock(_timingMutex);
                _scheduleInitialized = false;
                _nextIterationNanoseconds = 0;
                if (clearMeasurements) _resetMeasurementsLocked();
            }
            _signalScheduler();
        }

        void _recordSampleLocked(uint64_t startToStartDelta) {
            if (_iterationSampleCount == 0 || startToStartDelta == 0) return;
            _iterationFrequency =
                static_cast<double>(Timing::NanosecondsPerSecond) /
                static_cast<double>(startToStartDelta);
            _iterationSamples.push_back(startToStartDelta);
            while (_iterationSamples.size() > _iterationSampleCount) {
                _iterationSamples.pop_front();
            }
            long double total = 0.0L;
            for (uint64_t sample : _iterationSamples) total += static_cast<long double>(sample);
            _averageIterationFrequency = total > 0.0L
                ? static_cast<double>(
                    (static_cast<long double>(_iterationSamples.size()) *
                     Timing::NanosecondsPerSecond) / total
                  )
                : 0.0;
        }

        static uint32_t _getWaitMilliseconds(uint64_t remainingNanoseconds) {
            uint64_t milliseconds =
                remainingNanoseconds / Timing::NanosecondsPerMillisecond;
            if (milliseconds == 0) return 0;
            milliseconds = std::min<uint64_t>(milliseconds, UINT32_MAX - 1ULL);
            return static_cast<uint32_t>(milliseconds);
        }

    protected:
        /// <summary>Handles a scheduler wake requested independently of the periodic iteration schedule.</summary>
        virtual void OnWorkWake() {}

        /// <summary>Wakes the scheduler so <see cref="OnWorkWake"/> can run promptly.</summary>
        void WakeForWork() {
            _workWakeRequested.store(true);
            _signalScheduler();
        }

        /// <summary>Executes one scheduled precision-thread iteration.</summary>
        /// <param name="delta">Configured delta measurement for this iteration.</param>
        /// <param name="startTime">Clock time at which the iteration began.</param>
        /// <param name="skippedIterations">Number of schedule periods skipped because execution was behind.</param>
        virtual void Iterate(
            IterationTime delta,
            IterationTime startTime,
            SkippedIterationCount skippedIterations
        ) = 0;

        /// <summary>Implements the clock-driven scheduling loop used by the underlying Thread task.</summary>
        void OnLoop() final override {
            if (_workWakeRequested.exchange(false)) {
                OnWorkWake();
                return;
            }

            const uint64_t now = _getNowNanoseconds();
            uint64_t period = 0;
            uint64_t deltaNanoseconds = 0;
            uint64_t remainingNanoseconds = 0;
            uint64_t measurementGeneration = 0;
            bool shouldWait = false;
            SkippedIterationCount skippedIterations = 0;

            {
                std::lock_guard<std::mutex> lock(_timingMutex);
                period = _iterationPeriodNanoseconds;
                if (!_scheduleInitialized) {
                    _scheduleInitialized = true;
                    _nextIterationNanoseconds = now;
                }

                if (period > 0 && now < _nextIterationNanoseconds) {
                    remainingNanoseconds = _nextIterationNanoseconds - now;
                    shouldWait = true;
                } else {
                    if (_hasPreviousIteration) {
                        const uint64_t baseline =
                            _deltaMode == IterationDeltaMode::StartToStart
                                ? _previousStartNanoseconds
                                : _previousEndNanoseconds;
                        deltaNanoseconds = now >= baseline ? now - baseline : 0;
                        if (now >= _previousStartNanoseconds) {
                            _recordSampleLocked(now - _previousStartNanoseconds);
                        }
                    }

                    if (period > 0) {
                        const uint64_t behind = now - _nextIterationNanoseconds;
                        const uint64_t elapsedPeriods = behind / period;
                        skippedIterations = elapsedPeriods;
                        const uint64_t periodsToAdvance =
                            elapsedPeriods == std::numeric_limits<uint64_t>::max()
                                ? elapsedPeriods
                                : elapsedPeriods + 1;
                        const uint64_t advance =
                            periodsToAdvance > std::numeric_limits<uint64_t>::max() / period
                                ? std::numeric_limits<uint64_t>::max()
                                : periodsToAdvance * period;
                        _nextIterationNanoseconds =
                            _addSaturated(_nextIterationNanoseconds, advance);
                    }

                    _activeIterationStartNanoseconds = now;
                    measurementGeneration = _measurementGeneration;
                }
            }

            if (shouldWait) {
                const uint32_t waitMilliseconds = _getWaitMilliseconds(remainingNanoseconds);
                if (waitMilliseconds > 0 && _scheduleSignal != nullptr) {
                    (void)_scheduleSignal->Wait(waitMilliseconds);
                } else {
                    Task::TaskRuntime::Yield();
                }
                return;
            }

            const IterationTime delta = _fromNanoseconds(deltaNanoseconds);
            const IterationTime startTime = _fromNanoseconds(now);
            Iterate(delta, startTime, skippedIterations);
            const uint64_t end = _getNowNanoseconds();

            {
                std::lock_guard<std::mutex> lock(_timingMutex);
                if (measurementGeneration == _measurementGeneration) {
                    _previousStartNanoseconds = now;
                    _previousEndNanoseconds = end;
                    _hasPreviousIteration = true;
                }
            }

            _iterationObservable->Notify(this, delta, startTime, skippedIterations);
            if (period == 0) Task::TaskRuntime::Yield();
        }

    public:
        /// <summary>Constructs a precision thread using the supplied clock or the default Timing system clock.</summary>
        explicit PrecisionThread(ClockType* clock = nullptr)
            : _clock(
                clock == nullptr
                    ? &Timing::SystemClock<IterationTime>::GetInstance()
                    : clock
              ),
              _scheduleSignal(System::Synchronization::CreateBinarySignal()) {}

        /// <summary>Constructs a precision thread with an explicit release policy and optional clock.</summary>
        PrecisionThread(
            ThreadReleasePolicy releasePolicy,
            ClockType* clock = nullptr
        )
            : Thread(releasePolicy),
              _clock(
                clock == nullptr
                    ? &Timing::SystemClock<IterationTime>::GetInstance()
                    : clock
              ),
              _scheduleSignal(System::Synchronization::CreateBinarySignal()) {}

        ~PrecisionThread() override {
            Shutdown();
            _scheduleSignal.reset();
        }

        /// <summary>Resets scheduler state before initializing the underlying thread.</summary>
        ThreadInitializationStatus Initialize() override {
            _resetSchedule();
            return Thread::Initialize();
        }

        /// <summary>Starts or resumes execution and wakes the scheduler.</summary>
        ThreadInitializationStatus Start() override {
            if (GetThreadState() == ThreadState::Paused) _resetSchedule();
            const ThreadInitializationStatus status = Thread::Start();
            _signalScheduler();
            return status;
        }

        /// <summary>Pauses execution and resets schedule measurements.</summary>
        void Pause() override {
            Thread::Pause();
            _resetSchedule();
        }

        /// <summary>Requests termination and wakes any waiting scheduler.</summary>
        void Terminate() override {
            Thread::Terminate();
            _signalScheduler();
        }

        /// <summary>Advances the next scheduled iteration to the current clock time and wakes the scheduler.</summary>
        void Bump() {
            const uint64_t now = _getNowNanoseconds();
            {
                std::lock_guard<std::mutex> lock(_timingMutex);
                _nextIterationNanoseconds = now;
                _scheduleInitialized = true;
            }
            _signalScheduler();
        }

        /// <summary>Registers an observer for completed precision-thread iterations.</summary>
        Observable::ObserverHandlePtr RegisterIterationObserver(
            IPrecisionThreadObserver<TTime, TRepresentationTraits>* observer
        ) {
            return _iterationObservable->RegisterObserver(observer);
        }

        /// <summary>Unregisters an iteration observer.</summary>
        void UnregisterIterationObserver(
            IPrecisionThreadObserver<TTime, TRepresentationTraits>* observer
        ) {
            _iterationObservable->UnregisterObserver(observer);
        }

        /// <summary>Returns the clock used to schedule and measure iterations.</summary>
        ClockType* GetClock() const { return _clock; }

        /// <summary>Returns the current iteration delta measurement mode.</summary>
        IterationDeltaMode GetIterationDeltaMode() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return _deltaMode;
        }

        /// <summary>Sets the iteration delta measurement mode and resets accumulated measurements when changed.</summary>
        void SetIterationDeltaMode(IterationDeltaMode mode) {
            std::lock_guard<std::mutex> lock(_timingMutex);
            if (_deltaMode != mode) {
                _deltaMode = mode;
                _resetMeasurementsLocked();
            }
        }

        /// <summary>Returns the actual scheduler period between iteration starts.</summary>
        IterationTime GetIterationPeriod() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return _fromNanoseconds(_iterationPeriodNanoseconds);
        }

        /// <summary>Sets the actual scheduler period between iteration starts.</summary>
        void SetIterationPeriod(IterationTime period) {
            const uint64_t nanoseconds = _toNanoseconds(period);
            {
                std::lock_guard<std::mutex> lock(_timingMutex);
                _iterationPeriodNanoseconds = nanoseconds;
                if (
                    nanoseconds > 0 &&
                    _desiredIterationPeriodNanoseconds > 0 &&
                    _desiredIterationPeriodNanoseconds < nanoseconds
                ) {
                    _desiredIterationPeriodNanoseconds = nanoseconds;
                }
                _scheduleInitialized = false;
            }
            _signalScheduler();
        }

        /// <summary>Sets the scheduler period from an ESPressio Time value.</summary>
        template<typename TValue, Units::UnitOrderOfMagnitude TMagnitude>
        void SetIterationPeriod(const Units::Time<TValue, TMagnitude>& period) {
            static_assert(
                std::is_integral<TValue>::value && std::is_unsigned<TValue>::value,
                "Iteration periods require an unsigned integral ESPressio Time value"
            );
            const uint64_t nanoseconds = period.template ToMagnitude<uint64_t>(Units::Nano);
            SetIterationPeriod(
                Timing::TimeTraits<IterationTime>::template FromNanoseconds<uint64_t>(
                    nanoseconds,
                    1
                )
            );
        }

        /// <summary>Returns the desired execution budget used to calculate available iteration time.</summary>
        IterationTime GetDesiredIterationPeriod() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return _fromNanoseconds(_desiredIterationPeriodNanoseconds);
        }

        /// <summary>Sets the desired execution budget, clamped to at least the scheduler period when both are nonzero.</summary>
        void SetDesiredIterationPeriod(IterationTime period) {
            uint64_t nanoseconds = _toNanoseconds(period);
            std::lock_guard<std::mutex> lock(_timingMutex);
            if (
                _iterationPeriodNanoseconds > 0 &&
                nanoseconds > 0 &&
                nanoseconds < _iterationPeriodNanoseconds
            ) {
                nanoseconds = _iterationPeriodNanoseconds;
            }
            _desiredIterationPeriodNanoseconds = nanoseconds;
        }

        /// <summary>Sets the desired execution budget from an ESPressio Time value.</summary>
        template<typename TValue, Units::UnitOrderOfMagnitude TMagnitude>
        void SetDesiredIterationPeriod(const Units::Time<TValue, TMagnitude>& period) {
            static_assert(
                std::is_integral<TValue>::value && std::is_unsigned<TValue>::value,
                "Desired iteration periods require an unsigned integral ESPressio Time value"
            );
            const uint64_t nanoseconds = period.template ToMagnitude<uint64_t>(Units::Nano);
            SetDesiredIterationPeriod(
                Timing::TimeTraits<IterationTime>::template FromNanoseconds<uint64_t>(
                    nanoseconds,
                    1
                )
            );
        }

        /// <summary>Returns the number of start-to-start deltas retained for average-frequency measurement.</summary>
        uint32_t GetIterationSampleCount() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return _iterationSampleCount;
        }

        /// <summary>Sets the frequency sample window size and clears accumulated frequency measurements.</summary>
        void SetIterationSampleCount(uint32_t sampleCount) {
            std::lock_guard<std::mutex> lock(_timingMutex);
            _iterationSampleCount = sampleCount;
            std::deque<uint64_t>().swap(_iterationSamples);
            _iterationFrequency = 0.0;
            _averageIterationFrequency = 0.0;
        }

        /// <summary>Returns the frequency measured from the most recent start-to-start interval.</summary>
        IterationFrequency GetIterationFrequency() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return RepresentationTraits::CreateIterationFrequency(_iterationFrequency);
        }

        /// <summary>Returns the average iteration frequency across the configured sample window.</summary>
        IterationFrequency GetAverageIterationFrequency() const {
            std::lock_guard<std::mutex> lock(_timingMutex);
            return RepresentationTraits::CreateIterationFrequency(_averageIterationFrequency);
        }

        /// <summary>Returns the remaining execution budget for the active iteration, or a negative overrun when its deadline has passed.</summary>
        SignedIterationTime GetAvailableIterationTime() const {
            const uint64_t now = _getNowNanoseconds();
            std::lock_guard<std::mutex> lock(_timingMutex);
            if (
                _desiredIterationPeriodNanoseconds == 0 ||
                _activeIterationStartNanoseconds == 0
            ) {
                return RepresentationTraits::CreateSignedIterationTime(0);
            }

            const uint64_t deadline = _addSaturated(
                _activeIterationStartNanoseconds,
                _desiredIterationPeriodNanoseconds
            );

            if (deadline >= now) {
                const uint64_t remaining = deadline - now;
                return RepresentationTraits::CreateSignedIterationTime(
                    remaining > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                        ? std::numeric_limits<int64_t>::max()
                        : static_cast<int64_t>(remaining)
                );
            }

            const uint64_t overrun = now - deadline;
            return RepresentationTraits::CreateSignedIterationTime(
                overrun > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                    ? std::numeric_limits<int64_t>::min()
                    : -static_cast<int64_t>(overrun)
            );
        }
    };

}
}