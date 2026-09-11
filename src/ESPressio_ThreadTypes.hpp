#pragma once
#include <cstddef>
#include <cstdint>
#include <exception>
#include <ESPressio_TaskTypes.hpp>
namespace ESPressio::Threads {
/// <summary>Physical lifecycle; owner-controlled reinitialization is allowed only after successful Shutdown.</summary>
enum class ThreadState : std::uint8_t { Uninitialized, Initializing, Initialized, Running, Paused, Terminating, Terminated };
/// <summary>Expected lifecycle/resource outcomes, never an application failure exception.</summary>
enum class ThreadStatus : std::uint8_t {
    Success, AlreadyInitialized, InvalidState, Busy, InvalidConfiguration, StackTooSmall,
    StorageUnavailable, SignalUnavailable, UnsupportedProvider, TaskCreationFailed,
    InitializationFailed, SelfJoin, JoinFailed, GenerationExhausted
};
/// <summary>Application demand is independent of asynchronous capability readiness.</summary>
enum class ThreadWorkDisposition : std::uint8_t { IdleReady, ImmediateWorkRemaining };
/// <summary>No deadline is explicit; UINT64_MAX remains a representable absolute deadline.</summary>
struct ThreadDeadline final {
    bool Present=false;
    std::uint64_t MonotonicNanoseconds=0;
    static constexpr ThreadDeadline At(std::uint64_t now) noexcept { return {true,now}; }
    bool Due(std::uint64_t now) const noexcept { return Present && MonotonicNanoseconds<=now; }
    void Include(ThreadDeadline value) noexcept {
        if (value.Present && (!Present || value.MonotonicNanoseconds<MonotonicNanoseconds)) *this=value;
    }
};
/// <summary>One retained first fatal cause; capturing an existing exception does not wrap/allocate another exception.</summary>
enum class ThreadFailurePhase : std::uint8_t { None, Initialization, Lifecycle, Capability, Application, Teardown, Provider };
struct ThreadFailure final {
    ThreadFailurePhase Phase=ThreadFailurePhase::None;
    std::exception_ptr Cause{};
    std::uint64_t MonotonicNanoseconds=0;
};
/// <summary>Exact resident/caller storage and one maximum-path stack floor; unknown provider overhead stays unknown.</summary>
struct ThreadResourceProfile final {
    std::size_t ResidentBytes=0,ResidentAlignment=1;
    std::size_t ExternalStorageBytes=0,ExternalStorageAlignment=1;
    std::uint32_t FrameworkStackFloorBytes=0,ConfiguredStackBytes=0;
    bool PlatformControlBytesKnown=false;
    std::size_t PlatformControlBytes=0;
    std::uint32_t ExecutionContexts=1,CommonWorkSignals=1;
};
struct ThreadConfiguration final {
    Task::TaskExecutionConfiguration Execution{};
    /// <summary>Application's declared worst active path, combined by maximum with framework floors.</summary>
    std::uint32_t ApplicationStackFloorBytes=0;
};
struct ThreadDiagnostics final {
    ThreadState State=ThreadState::Uninitialized;
    ThreadFailure Failure{};
    std::uint64_t ServiceCycles=0,ApplicationIterations=0,Waits=0;
    std::uint32_t MinimumFreeStackBytes=0;
    bool StackTelemetryAvailable=false;
};
}
