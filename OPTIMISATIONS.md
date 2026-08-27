# ESPressio Threads Optimisation Log

This file records the current resource-optimisation round chronologically. Version numbers are intentionally unchanged during this round.

Rollback anchor: `rollback/optimisations-pre-20260825` -> `20f8579142fa6a4e0da25643748adf3ec25f33d8`.

## 2026-08-25 — Optimisation round opened (#69)

### Context
Full-stack ESP32 hardware validation exposed severe internal-RAM pressure when Threads, Event, WiFi, ESP-NOW, serialization and device hardware coexist. Threads is a systemic target because every `Thread` contributes task-stack reservation and repeated synchronization/lifecycle infrastructure.

### Planned low-risk/high-gain work
- Measure and reduce over-conservative task-stack defaults only where a safe margin can be demonstrated.
- Consolidate scalar configuration/state synchronization where equivalent concurrency guarantees can be preserved with atomics or one compact lock.
- Remove unnecessary heap/shared ownership only where the underlying type's ownership contract permits it.
- Preserve initialization, lifecycle, observer, termination-dispatch, garbage-collection and callback reentrancy semantics.
- Add regression/stress coverage before accepting concurrency-sensitive changes.

### Safety / rollback
No optimisation is accepted solely for memory savings. Changes must preserve behavioral and thread-safety guarantees. The rollback branch above remains an immutable pre-optimisation reference.

## 2026-08-25 — Right-size termination-dispatch queue (#69)

The default `ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH` was reduced from 256 to 32.

### Rationale
Each queue entry contains a `Thread*` and `ThreadManagerThreadSnapshot`. Reserving 256 entries consumes several kilobytes of internal RAM even on systems with only a small number of Threads. The termination path already uses `xQueueSend(..., portMAX_DELAY)`, so queue saturation applies backpressure rather than dropping a termination request. A 32-entry default is therefore a substantially smaller bounded reservation without weakening delivery semantics.

The macro remains overrideable for unusually large workloads.

### Verification
A new `Resource Profile Regression` PlatformIO workflow compiles both the default profile and an explicit 64-entry override, guarding both the new default and continued configurability.

Commits: `6bfa8d8707cdc509285390b32842205d848794a0`, `1d9beac867f53e384dcbc9d1d10cc80f73c6c530`.

## 2026-08-25 — Attempted direct ownership of process-lifetime infrastructure observables (#69)

`ThreadTerminationDispatcher` and `ThreadGarbageCollector` were temporarily changed to own their Observable-derived helper objects directly instead of through `std::shared_ptr`/`std::make_shared`.

### Original rationale
Both owners are process-lifetime singletons, so shared lifetime management initially appeared unnecessary. The intent was to remove two heap allocations, two shared-pointer control blocks and associated startup fragmentation.

Commits: `bb5dede5fb4665beaccccf4900bf04b2f72a84d7`, `b40669729c6881d215d6690d0b056be1b0329487`, `5541d5ce61f358f679651a085f75ce85cd4150f5`.

## 2026-08-25 — Hardware rollback: Observable ownership contract requires shared ownership (#69)

The direct-ownership optimisation above was invalid and has been reverted.

### Evidence
The optimisation-branch Lab boot crashed in the termination dispatcher. Symbol resolution of the exact ELF showed:

`ThreadTerminationDispatcher::_loop()` -> `DispatcherObservable::Started()` -> `ThreadSafeObservable::ExecuteNotification()` -> `IObservable::AcquireNotificationLifetime()` -> `ObservableOwnershipException` -> `std::terminate()`.

`IObservable::AcquireNotificationLifetime()` deliberately calls `shared_from_this()` and converts `std::bad_weak_ptr` into `ObservableOwnershipException`. Notification-capable Observable instances therefore have a fundamental shared-ownership contract; process lifetime alone is not sufficient grounds for direct member ownership.

### Correction
`ThreadTerminationDispatcher::DispatcherObservable` and `ThreadGarbageCollector::GarbageCollectorObservable` are again owned by `std::shared_ptr` created with `std::make_shared`.

The 32-entry termination queue and all unrelated optimisation work remain in place.

Corrective commits: `ca1d043bad4d95a092ee3cdd352294323641f047`, `05eba11539633597a7141d1cc184afb4e88ecf6f`.

### Safety rule added
No Observable-derived object may be converted from shared ownership to direct/stack/member ownership unless the Observable library's notification-lifetime contract is first changed explicitly and regression-tested. This optimisation class is therefore excluded from the current round.

## 2026-08-25 — Downstream working branches pinned to Threads #69

The active Event, WiFi and ESP-Now working branches now resolve ESPressio Threads directly from `optimisation/69-resource-footprint` so their CI/hardware testing cannot accidentally validate against released 3.1.7 while this optimisation round is active.

### Deferred candidate: per-Thread scalar/configuration wrapper consolidation

A high-gain candidate remains: `Thread` currently carries separate `ReadWriteMutex` wrappers for `freeOnTerminate`, `startOnInitialize`, stack size, priority and core ID in addition to an existing `_taskConfigurationMutex`. Each wrapper contains a `std::shared_mutex` plus callback/comparison `std::function` state. Consolidating these is expected to produce meaningful per-Thread savings.

This has deliberately not been bundled into the first tranche because `_initialize()` currently holds `_taskConfigurationMutex` while calling the public configuration getters. A safe implementation therefore requires an explicit configuration snapshot/atomic design plus race/reentrancy tests; simply adding getter locking would deadlock.

### Validation note
The earlier CI checkpoint was green but did not exercise the Observable notification ownership path that failed on device. Hardware validation therefore caught a real coverage gap. Future acceptance of ownership-related optimisations requires a test that actually invokes notifications under the proposed ownership model.

## 2026-08-25 — Lazy ThreadSafe callback/comparator storage (#69)

`Mutex<T>` and `ReadWriteMutex<T>` previously embedded two `std::function` members in every instance: an on-change callback and comparator. Most ESPressio scalar wrappers use neither custom facility, yet every instance permanently paid the object-size cost.

The callback/comparator pair is now held in lazily allocated `ThreadSafeCallbacks<T>` storage only when a caller supplies at least one custom function. The normal case compares with `operator==` directly and retains only a nullable `unique_ptr` in addition to the value and lock.

### Behavior preserved
- `Get`/`TryGet`, read/write locking and shared-reader semantics are unchanged;
- arbitrary custom comparators and on-change callbacks remain supported;
- callbacks are still copied under the lock and invoked after the lock is released;
- try-lock operations retain their existing failure semantics.

This is deliberately a first stage toward per-Thread scalar consolidation because it reduces every default wrapper system-wide without changing `Thread`'s configuration-lock ordering.

Commit: `d2a6198c7fc763ea9a7ca80d3a0b841c59ee88ae`.

## 2026-08-25 — Lazy termination/garbage-collection infrastructure (#71)

Hardware validation showed the full WiFi + ESP-NOW Lab reaching Ready with effectively no allocatable internal DRAM while the termination dispatcher and garbage collector had already reserved worker stacks and FreeRTOS infrastructure despite no termination or cleanup work having occurred.

### Changes
- `ThreadTerminationDispatcher` leaves its queue/task unallocated at singleton construction and creates them on demand.
- `ThreadGarbageCollector` likewise leaves its binary semaphore/task unallocated until the first `CleanUp()` request.
- availability inspection does not itself force allocation;
- observers receive initialization success/failure when physical infrastructure is actually attempted;
- blocking termination-queue backpressure, dispatcher-owned task deletion, GC request coalescing and synchronous GC fallback remain intact.

### Expected steady-state saving
Where lifecycle semantics permit infrastructure to remain dormant, applications avoid the corresponding 2000-byte infrastructure stack reservation plus queue/semaphore and FreeRTOS task-control overhead until required.

Original commits: `da7f91f7efa10f3d68633d74056a3a5e3594c3a3`, `09239db43afb8956893010dd1d56aff834e42dd9`, `995cba867a01aecb17818d0f57014e2928a070ed`.

## 2026-08-25 — Hardware correction: Thread initialization requires termination infrastructure (#71)

StickB hardware validation showed `EventTransportManager early initialization status=4`. `ThreadInitializationStatus` value 4 is `TerminationDispatcherUnavailable`: `Thread::Initialize()` requires working termination-dispatch infrastructure before a normal ESPressio task may start. The original lazy implementation only created the dispatcher from `Dispatch()`, making startup correctness depend on incidental initialization order.

### Correction
`ThreadTerminationDispatcher::EnsureAvailable()` now demand-initializes the infrastructure, and `Thread::_isTerminationDispatcherAvailable()` uses that method during Thread initialization. This preserves the Thread lifecycle invariant while still avoiding eager singleton-constructor allocation before the first actual Thread needs the service.

Corrective commits: `2bdeed022bf98eea0ba70d62557d2d610ebcee08`, `0e65d68de8381ac740a26d0d5f6ef44ec6e90cca`, `a645ef8dba073bc73fde9ab7d135cb973506ecf6`.

### Revised optimisation conclusion
Under the present Thread lifecycle design, the termination dispatcher cannot remain unallocated once the first normal ESPressio Thread starts. It is therefore not counted as a steady-state saving. Garbage collection remains independently demand-driven.

## 2026-08-25 — Hardware correction: publish termination queue before task creation (#71)

The first `EnsureAvailable()` hardware build exposed a second, cross-core initialization race. StickB immediately rebooted with FreeRTOS assertion `xQueueReceive queue.c:1531 (( pxQueue ))`. Exact ELF/MAP resolution placed the failing frames in `ThreadTerminationDispatcher::_loop()` and `_taskEntry()`.

### Root cause
`_initialize()` created the queue into a local variable, then called `xTaskCreate()`, and only after `xTaskCreate()` returned assigned `_queue = queue`. On a dual-core ESP32 the newly-created dispatcher task may become runnable immediately on the other core. It could therefore enter `_loop()` and execute `xQueueReceive(_queue, ...)` while the member was still null.

### Correction
- publish `_queue` while holding `_initializationMutex` before calling `xTaskCreate()`;
- `IsAvailable()` still requires both `_queue` and `_taskHandle`, so lifecycle callers cannot observe the half-created dispatcher while the mutex is held;
- if task creation fails, reset `_queue` to null and delete the temporary queue before reporting initialization failure.

This preserves demand initialization while making startup ordering safe across cores.

Corrective commit: `78b3085e9a353b35b4bc17e39c19e535cc373109`.

### Remaining Threads work
The five per-Thread scalar wrappers remain candidates for a later explicit configuration-snapshot redesign. Generic task-stack defaults are not being reduced blindly; subsequent reductions must use task-specific hardware high-water evidence with a retained safety reserve.

## 2026-08-25 — ESPressio Task backend migration (#72)

The new ESPressio Task library now owns the low-level FreeRTOS task-execution primitive. Threads remains the higher-level lifecycle-managed persistent-worker abstraction.

### Completed migration
- `ThreadTerminationDispatcher` creates its worker through `Task::TaskRuntime::Create()`, uses `TaskRuntime::Current()` for ownership checks and exits through `TaskRuntime::Delete()`.
- `ThreadGarbageCollector` creates and exits its worker through the same Task runtime abstraction.
- Both migrations preserve the existing lazy allocation, queue/semaphore backpressure, cross-core publication ordering and lifecycle/observer contracts.
- `library.json` resolves ESPressio Task directly from `feature/1-task-execution` during this coordinated development round.
- Added CI guards which reject direct `xTaskCreate*`/`vTaskDelete` ownership returning to the migrated infrastructure workers, plus an ESP32 integration compile against the Task working branch.

### Deliberately unchanged
FreeRTOS queues, semaphores, task notifications and delay/yield primitives remain in Threads where they define Thread lifecycle/scheduling semantics. #72 is an ownership/backend migration, not a rewrite of the Thread state machine onto a generic executor.

### Remaining #72 work
The primary `Thread` task itself still uses direct FreeRTOS create/delete/current/suspend operations. That conversion is the remaining task-ownership site and must preserve its start-gate notification, initialization race handling, dispatcher-owned deletion and current-task termination semantics. It will be migrated separately rather than folded into the already-validated infrastructure-worker change.

### Commits
- `3bddd28` — `refactor(#72): create termination worker through ESPressio Task runtime`
- `32fa75e` — `refactor(#72): create garbage collector worker through ESPressio Task runtime`
- `c021d75` / `fe44756` — working-branch Task dependency metadata
- `51c05ec` — `test(#72): guard ESPressio Task ownership of infrastructure workers`

## 2026-08-25 — Garbage collector task retired; reclamation absorbed by dispatcher (#72)

Full-stack hardware profiling showed that the dedicated `ThreadGarbageCollector` represented an avoidable internal-RAM reservation on a system already reaching ESP-NOW `ESP_ERR_ESPNOW_NO_MEM`. Architectural review confirmed that the collector did not provide a unique reclamation mechanism: its ultimate operation was `ThreadManager::CleanUpWithResult()`, while the termination dispatcher already supplied the independent execution context required to delete a Thread only after its own native task stack had stopped executing.

### New lifecycle
- a terminating Thread still suspends and queues exactly one termination-dispatch record;
- `ThreadTerminationDispatcher` deletes the native worker task, delivers `OnTerminated` / `OnThreadTaskExited`, and releases shutdown waiters;
- after `_dispatchTermination()` returns, the dispatcher invokes `ThreadManager::CleanUpWithResult()`;
- `ThreadManager` remains the sole owner of automatic cleanup eligibility, atomic claim, registry removal, deferred-iteration handling, and C++ object deletion;
- cleanup is requested after every completed termination so an `OnTerminated` callback may still change `FreeOnTerminate` before manager eligibility is evaluated;
- after manager cleanup begins, the dispatcher never dereferences the raw `Thread*` again and uses only the captured value snapshot for observer completion.

### Removed infrastructure
The dedicated garbage-collector task, its 2000-byte stack, binary semaphore, TaskRuntime worker, GC-specific observable/observer/result API and standalone example have been removed. The historical `Thread::GarbageCollect()` method remains as a source-compatibility shim but now delegates directly to `ThreadManager` and allocates no worker infrastructure.

### Concurrency semantics preserved
`ThreadManager::CleanUpWithResult()` already defers cleanup when `_activeIterations > 0`; the final manager `IterationGuard` performs the pending cleanup later. A second task is therefore unnecessary for safe deferral. Explicit-release Threads remain unclaimed, and `ReleaseOnTerminate` objects remain protected by `TryClaimAutomaticCleanup()`.

### Expected resource saving
Once the previous GC path would have initialized, the new architecture permanently avoids the collector's 2000-byte task stack plus FreeRTOS task-control allocation, semaphore, shared observable state and scheduling/context-switch overhead.

### Validation
The Task-runtime CI guard now rejects reintroduction of any `*GarbageCollector*` source file, requires dispatcher-to-manager cleanup, and compiles both explicit-release and automatic-release Thread construction/lifecycle paths against the Task working branch.

## 2026-08-27 — System-backed ThreadManager registry and snapshots (#75)

Phase 7 of the coordinated memory-policy programme moves ESPressio-owned ThreadManager bookkeeping away from scarce internal RAM while preserving the RTOS ownership boundary.

### Changes
- the variable-size ThreadManager registry now uses ESPressio-System `ExternalPreferred` storage;
- `ForEachThread`, initialization and cleanup lock-boundary snapshots use external-preferred storage rather than the default heap;
- cleanup claim bookkeeping uses external-preferred storage and the complete ThreadRecord snapshot is released once cleanup claims have been resolved;
- `library.json` resolves ESPressio-System directly from `feature/1-system-memory-policy` during coordinated validation.

### Deliberately unchanged
FreeRTOS task stacks, queues, task-control structures and other RTOS/native synchronization storage remain internal. Snapshots are retained where required to avoid holding manager locks across virtual/user code; their placement and lifetime were changed rather than weakening that concurrency boundary.

### Commits
- `f1c2033` — `optimise(#75): externalise ThreadManager registry and snapshots`
- `00b3351` — working-branch System dependency metadata
