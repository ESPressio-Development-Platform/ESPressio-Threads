# ESPressio Threads Optimisation Log

This file records the current resource-optimisation round chronologically. Version numbers are intentionally unchanged during this round.

Rollback anchor: `rollback/optimisations-pre-20260825` -> `20f8579142fa6a4e0da25643748adf3ec25f33d8`.

## 2026-08-25 — Optimisation round opened (#69)

### Context
Full-stack ESP32 hardware validation exposed severe internal-RAM pressure when Threads, Event, WiFi, ESP-NOW, serialization and device hardware coexist. Threads is a systemic target because every `Thread` contributes task-stack reservation and repeated synchronization/lifecycle infrastructure.

### Planned low-risk/high-gain work
- Measure and reduce over-conservative task-stack defaults only where a safe margin can be demonstrated.
- Consolidate scalar configuration/state synchronization where equivalent concurrency guarantees can be preserved with atomics or one compact lock.
- Remove unnecessary heap/shared ownership for per-Thread lifecycle observable state where object lifetime already provides ownership.
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

## 2026-08-25 — Remove heap ownership from process-lifetime infrastructure observables (#69)

`ThreadTerminationDispatcher` and `ThreadGarbageCollector` were changed to own their observable objects directly instead of through `std::shared_ptr`/`std::make_shared`.

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

This has deliberately not been bundled into the first tranche because `_initialize()` currently holds `_taskConfigurationMutex` while calling the public configuration getters. A safe implementation therefore requires an explicit configuration snapshot/atomic design plus race/reentrancy tests; simply adding getter locking would deadlock. It will be addressed as a separate #69 change after this green checkpoint.

### Validation checkpoint
Before the ownership fault was exposed by hardware, the existing Critical TLS / Task Exit Regression, including ESP32 pthread and WiFi-driver coexistence compilation, completed successfully on the optimisation branch. The hardware failure demonstrates that compile/host coverage did not exercise this notification-ownership path; future regression coverage must include actual notification invocation for infrastructure Observables.
