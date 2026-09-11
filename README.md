# ESPressio Threads

`Threads::Thread` owns one persistent execution context, one configured stack and
one common latched work signal. `ThreadWith<Capabilities...>` is the sole nonempty
capability host. `Precision<N>` adds monotonic cadence and fixed telemetry without
creating another task, signal, callback loop or runtime registry.

Consume `ESPressio-Threads` from `primitives_redesign` with coordinated System,
Task, Timing and Units checkouts. The existing package version is unchanged during
this architectural reset. Threads has no Primitive-family or Observable dependency.
Platform execution and synchronization providers must be installed before initialization
and outlive the Thread object. Joinable execution is required; unsupported providers
return `UnsupportedProvider` rather than deleting a running task.

## Bare Thread and explicit ownership

Initialize freezes resources and bindings but leaves execution blocked. Start
activates the initialized topology. An ordinary `OnLoop` reports whether it needs
another iteration. `RequestLoop()` publishes application demand and wakes the root;
`Wake()` alone requests readiness reevaluation and does not count callbacks.

```cpp
#include <ESPressio_Thread.hpp>
using namespace ESPressio::Threads;
class Counter final : public Thread {
    unsigned count=0;
protected:
    ThreadWorkDisposition OnLoop() override {
        if (++count==10) {
            Terminate();
            return ThreadWorkDisposition::IdleReady;
        }
        return ThreadWorkDisposition::ImmediateWorkRemaining;
    }
public:
    ~Counter() override { (void)Shutdown(); }
};
void RunCounter() {
    Counter counter;
    if (counter.Initialize()!=ThreadStatus::Success) return;
    (void)counter.Start();
    // Owner may wait for application completion, or request cooperative shutdown.
    (void)counter.Shutdown();
}
```

The concrete owner must call `Shutdown()` before destroying members used by its
application hooks. The host joins before capability members are destroyed. A call
from the worker itself requests termination and returns `SelfJoin`; an external
owner completes the join. Concurrent owner operations return `Busy`. A failed join
retains the context for retry. No operation automatically deletes the C++ object.

## Precision cadence

All cadence values and contexts are raw monotonic nanoseconds. Only an
`IMonotonicClock` may be injected into the root. System synchronization, timeline
rebases and slews cannot move this clock. A positive cadence contributes a deadline;
an early asynchronous wake cannot run the application ahead of that deadline.

```cpp
#include <ESPressio_ThreadWith.hpp>
#include <ESPressio_Precision.hpp>
using namespace ESPressio::Threads;
class Sampler final : public ThreadWith<Precision<8>> {
protected:
    ThreadWorkDisposition OnLoop() override {
        const auto& iteration=GetCapability<PrecisionTag>().CurrentIteration();
        // Bounded sampling work uses StartTime, Delta, Lateness and SkippedIterationCount.
        (void)iteration;
        return ThreadWorkDisposition::IdleReady;
    }
public:
    Sampler() { GetCapability<PrecisionTag>().SetCadencePeriod(100000000); }
    ~Sampler() override { (void)Shutdown(); }
    void SampleSoon() { GetCapability<PrecisionTag>().Bump(); }
    PrecisionTelemetry Telemetry() const { return GetCapability<PrecisionTag>().GetTelemetry(); }
};
static_assert(Sampler::HasCapability<PrecisionTag>());
```

Start and Resume make the first iteration immediately eligible with zero delta and
no paused-time skipped count. Later iterations advance from nominal slots; late
iterations report skipped slots and never run a catch-up burst. Changing a positive
cadence reanchors to now plus the new period. `Bump()` forces a slot now and reanchors
subsequent cadence. Period zero removes the cadence restriction and uses ordinary
application work-disposition semantics.

`PrecisionDeltaMode` selects StartToStart or EndToStart measurement. The fixed ring
has compile-time maximum capacity and a runtime active window no larger than that
maximum (zero disables history). Sample insertion and rolling-frequency calculation
are O(1). Execution budget is measured from actual start and does not alter cadence.
`CurrentIteration()` is a read-only reference valid for the active owning OnLoop;
other contexts use the copied `GetTelemetry()` snapshot. If the next absolute slot
cannot be represented without uint64 overflow, `CadenceExhausted` reports it and
that cadence becomes dormant; no wrap or immediate catch-up loop occurs.

## Neutral capabilities

A capability declares its own unique `CapabilityTag` and the static protocol in
`ESPressio_ThreadCapability.hpp`. It may derive from the default protocol helper,
but never from Thread. Exclusive claims are neutral Types. Duplicate tags,
incompatible claims and malformed descriptors are compile errors. The pack is a
set: declaration order establishes no public priority.

```cpp
#include <atomic>
#include <ESPressio_ThreadWith.hpp>
using namespace ESPressio::Threads;
struct NoticeTag {};
struct Notice final : ThreadCapability {
    using CapabilityTag=NoticeTag;
    std::atomic<bool> Pending{false};
    ThreadHostServices Host{};
    ThreadStatus Initialize(const ThreadHostServices& host) { Host=host; return ThreadStatus::Success; }
    CapabilityReadiness Readiness(const ThreadCycleContext&) const noexcept {
        return {Pending.load(std::memory_order_acquire),{},true,false};
    }
    bool Publish() {
        ThreadAdmissionClaim claim(Host);
        if (!claim) return false;
        Pending.store(true,std::memory_order_release);
        claim.Release(); Host.Wake(); return true;
    }
    void Service(const ThreadCycleContext&) { (void)Pending.exchange(false); /* one bounded quantum */ }
    void Quiesce(const ThreadCycleContext&) noexcept { Pending.store(false); }
};
class NoticeWorker final : public ThreadWith<Notice> {
public:
    ~NoticeWorker() override { (void)Shutdown(); }
    void Notify() { GetCapability<NoticeTag>().Publish(); }
};
```

This coalesced notice is only a neutral protocol example. Family capabilities own
their prescribed FIFO/bitmap/response semantics and are supplied by their respective
libraries. Topology and typed bindings are finalized transactionally at Initialize;
a capability must reject later binding mutations. Partial initialization rolls back
entered capabilities in reverse traversal, including the failing capability.

`Readiness` is a noexcept, cheap, side-effect-free predicate: no mutex, queue walk,
bitmap scan, value copy, decode, allocation, application callback or extra clock read
on the negative path. Deadline contributors declare `NeedsMonotonicTime=true` and
return one absolute deadline or explicit absence. They use cycle-supplied `Now`.
Positive Service handles exactly one bounded natural quantum. Each sibling gets a
rotating turn; a due application cadence runs before another asynchronous quantum.
`BeforeApplication` may reject a stale eligibility decision after scalar tuning,
and must then leave application context unprepared. Lifecycle hooks are bounded;
post-initialization exceptions are fatal to the whole physical Thread.

## Wake, pause and failure

Publish readiness, deadline or lifecycle state before calling the common `Wake`
or `WakeFromInterrupt`. Cardinality lives in capability-owned records; the signal
may coalesce. The root never resets the latch between its final readiness check and
wait. Future deadlines use an interruptible platform wait, retaining nanosecond
absolute semantics; the current System timeout adapter rounds the wait upward to
milliseconds and reports actual lateness through Precision. It does not busy poll.

Pause prevents new quantum claims while a current bounded quantum may finish.
Capability ingress may remain boundedly active while paused. Resume runs activation
before permitting OnLoop. Termination closes new claims and admission through
`HostServices::IsAccepting`; this is a fast state snapshot. Ordinary producers use
`ThreadAdmissionClaim` around their bounded publication to linearize admission
against termination, then release the claim before Wake. Claim failure is a typed
nonblocking admission failure, not proof of inbox exhaustion. Capabilities unregister ingress and release pending
ownership in Quiesce. No generic callback drain, replay or restart is performed.

The first fatal application/capability/lifecycle cause is retained in a neutral
failure snapshot. All capabilities are quiesced even if another teardown hook
throws; secondary teardown failures cannot replace the first cause. Explicit
successful Shutdown permits owner-driven reinitialization.

## Resource accounting and validation

```cpp
#include <ESPressio_ThreadWith.hpp>
#include <ESPressio_Precision.hpp>
using namespace ESPressio::Threads;
void InspectResources() {
    ThreadConfiguration configuration;
    configuration.Execution.StackSize=4096;
    configuration.Execution.Core=0;
    configuration.ApplicationStackFloorBytes=2048;
    ThreadWith<Precision<8>> worker(configuration);
    const auto resource=GetThreadResourceProfile(worker);
    // ResidentBytes == sizeof(worker), plus explicitly reported caller storage.
    // One shared stack floor is max(root, capability paths, application declaration).
    (void)resource;
}
```

`GetThreadResourceProfile(concrete)` reports the exact concrete object size and
alignment. The member `GetResourceProfile()` reports the framework base/host size;
use the free typed function to include application members. Capability external
storage requirements are additive and individually validated at initialization.
There is no automatic resizing or hidden allocation fallback. Root and Precision
currently declare 1024-byte framework stack floors; application work declares its
own additional maximum path. Native control-block overhead is explicitly unknown
until the concrete provider supplies evidence. These floors do not certify a
particular application's or target compiler's worst-case stack use.

The common work signal is created at initialization and, after a successful
initialization, retained until object destruction, so stale Wake calls within the
object lifetime cannot race signal reclamation. Failed first initialization releases
its provisional signal. Join completion resources belong to the System execution
provider and are creation-time resources, separate from this one work signal.

Native validation uses the actual coordinated Task/System/Timing sources and the
Task test provider. Tests cover lifecycle rollback/join/faults, allocation denial,
static composition, rotating fairness, cadence precedence and fixed-ring telemetry.
See [the semantic migration record](docs/TH_VALIDATION.md) and
[the lifecycle contract](docs/THREAD_LIFECYCLE.md). ESP32 compilation covers pthread
and WiFi-driver coexistence. Physical stack, ISR and timing certification require
the later concrete-provider integration campaign.
