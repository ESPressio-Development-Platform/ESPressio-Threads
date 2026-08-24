# Changelog

## 3.1.7 — 2026-08-24

### Critical stability correction
- Replaced the originally published 3.1.7 task-exit implementation after hardware testing proved that ESPressio Threads occupied FreeRTOS thread-local-storage index 0, which is reserved by ESP-IDF pthread internals.
- Removed `ESPRESSIO_THREAD_TLS_INDEX` and all use of `vTaskSetThreadLocalStoragePointerAndDelCallback(...)` from ESPressio Threads.
- ESPressio-created tasks no longer write, reserve, inspect, or depend upon generic FreeRTOS TLS slots.
- Added an explicit, idempotent ESPressio-owned task-exit finalization path.
- A normally exiting ESPressio task now reaches `Terminated`, queues the termination dispatcher, and suspends; the dispatcher deletes the underlying FreeRTOS task from a separate context before `OnThreadTaskExited`, `OnTerminated`, shutdown release, or automatic garbage collection can proceed.
- Initialization-failure and termination-during-initialization paths explicitly delete the still-gated worker and enter the same deferred termination-dispatch contract.
- Termination dispatch may now wait for queue capacity because dispatch no longer occurs inside a FreeRTOS task-deletion callback.
- Direct external `vTaskDelete()` of an ESPressio-owned task is now explicitly unsupported. ESPressio task lifetime must be controlled through the ESPressio Threads lifecycle API (`Terminate()`, `Shutdown()`, manager/GC ownership).
- Added permanent CI guards rejecting future generic FreeRTOS TLS ownership, plus ESP32 compile regression coverage for pthread and WiFi-driver calls from an ESPressio Thread.

### Release replacement policy
- **The version remains 3.1.7 intentionally.** The originally published 3.1.7 release is considered critically unstable and is wholly invalidated by this correction.
- Normal semantic-versioning rules are intentionally overridden for this one critical correction: the existing 3.1.7 release/tag is expected to be mutated/reissued to point at the corrected implementation rather than creating a new downstream dependency cascade.
- This correction changes internal task-exit mechanics and formalizes direct external FreeRTOS task deletion as unsupported. No normal ESPressio `IThread` lifecycle method is removed, but applications that bypassed ESPressio by calling `vTaskDelete()` directly on an ESPressio task must stop doing so.

### Changed
- Raised required ESPressio Timing from `>=2.2.7 <3.0.0` to `>=2.2.8 <3.0.0`, propagating the Units 0.2.7 / Serializable 0.11.3 generation downstream.
- Preserved the required ESPressio Observable baseline at `>=3.0.2 <4.0.0`.
- Updated package metadata and CI validation for Threads 3.1.7.
- Updated explicit ESP32 compile validation of the opt-in `ESPressio_PrecisionThread_Serializable.hpp` surface against Timing 2.2.8, Units 0.2.7 and Serializable 0.11.3.
- Preserved the core dependency boundary: Threads depends directly on Timing and Observable only; Serializable support remains opt-in through Serializable Unit time/frequency representations.
- Updated README and current dependency documentation for the new cascade generation.

### Compatibility
- Threads retains no direct Serializable dependency; Serializable PrecisionThread representations remain opt-in.
- Ordinary callers using the ESPressio lifecycle API remain source-compatible.
- The original claim that 3.1.7 introduced no runtime behaviour changes is superseded by the critical correction above.

### Cascade
- Continues the release train: `Serializable 0.11.3 -> Units 0.2.7 -> Timing 2.2.8 -> Threads 3.1.7 -> Event 6.0.3 -> downstream integrations`.

## 3.1.6 — 2026-08-23

### Changed
- Raised required ESPressio Timing from `>=2.2.5 <3.0.0` to `>=2.2.7 <3.0.0`, propagating the corrected Units 0.2.6 / Serializable 0.11.2 generation downstream.
- Updated package metadata and CI validation for the corrected 3.1.6 cascade generation.
- Added explicit ESP32 compile validation of the opt-in `ESPressio_PrecisionThread_Serializable.hpp` surface with Timing 2.2.7, Units 0.2.6 and Serializable 0.11.2.
- Preserved the core dependency boundary: Threads depends directly on Timing and Observable only; Serializable support remains opt-in through Serializable Unit time/frequency representations.

### Compatibility
- Threads retains no direct Serializable dependency; Serializable PrecisionThread representations remain opt-in.
- No public Threads API or runtime behaviour changes are introduced by this dependency-maintenance release.

## 3.1.5 — 2026-08-22

### Changed
- Published the post-migration ESPressio Threads package generation from `ESPressio-Development-Platform`.
- Raised required ESPressio Timing from `>=2.2.4 <3.0.0` to `>=2.2.5 <3.0.0`.
- Raised required ESPressio Observable from `>=3.0.1 <4.0.0` to `>=3.0.2 <4.0.0`.
- Updated package metadata, README installation/dependency guidance, CI validation, and dependency documentation.

### Compatibility
- No Threads public API or runtime behaviour changes are introduced by this repository-relocation patch release.

All notable changes to this project are documented in this file.

The structure follows the principles of [Keep a
Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic
Versioning](https://semver.org/).

> **Historical note:** This changelog was reconstructed retrospectively
> from published GitHub Releases, tags, release notes, repository
> history, and the documented public API. Where an historical release
> had little or no release-note detail, the entry is intentionally terse
> rather than inferring unsupported intent.

## [3.1.4] - 2026-08-21

### Changed

- Raised the required ESPressio Timing baseline from 2.2.3 to 2.2.4, carrying the Units 0.2.3 / Serializable 0.10.2 dependency refresh downstream.
- Preserved the direct ESPressio Observable baseline at `>=3.0.1 <4.0.0`.
- Updated package and ESP-IDF component version metadata for Threads 3.1.4.
- Updated README and current dependency documentation to the 3.1.4 dependency generation.
- Preserved the dependency direction `Threads -> Timing -> Units -> optional Serializable`; Threads does not acquire a direct Serializable dependency.

### Compatibility

- No public Threads interfaces or runtime semantics changed.

## [3.1.3] - 2026-08-20

### Changed

- Raised the required ESPressio Timing baseline from 2.2.2 to 2.2.3, carrying the Units 0.2.2 / Serializable 0.10.1 dependency refresh downstream.
- Preserved the direct ESPressio Observable baseline at `>=3.0.1 <4.0.0`.
- Updated package and ESP-IDF component version metadata for Threads 3.1.3.
- Preserved the dependency direction `Threads -> Timing -> Units -> optional Serializable`; Threads does not acquire a direct Serializable dependency.

### Compatibility

- No public Threads interfaces or runtime semantics changed.

## [3.1.2] - 2026-08-20

### Changed

-   Raised the minimum ESPressio Timing dependency to 2.2.2, carrying forward the Observable 3.0.1 baseline from Timing's dependency-refresh patch.
-   Raised the direct ESPressio Observable dependency floor from 3.0.0 to 3.0.1.
-   Updated package and ESP-IDF component version metadata for Threads 3.1.2.
-   No public Threads interfaces or runtime semantics changed.

## [3.1.1] - 2026-08-19

### Changed

-   Updated the required ESPressio Timing baseline to 2.2.1, matching the dependency-refresh release generation.
-   Bounded ESPressio Timing compatibility to the current 2.x major line (`>=2.2.1 <3.0.0`).
-   Bounded ESPressio Observable compatibility to the current 3.x major line (`>=3.0.0 <4.0.0`).
-   Updated package metadata and current installation/dependency guidance for Threads 3.1.1.

## [3.1.0] - 2026-08-19

### Added

-   Added `IThreadManagerObserver`.
-   Added `IThreadGarbageCollectorObserver`.
-   Added `IThreadTerminationDispatcherObserver`.
-   Added immutable Thread Manager thread snapshots.
-   Added garbage-collection result snapshots and cleanup-result
    reporting.
-   Added Thread Manager initialization summaries.
-   Added observation of thread registration/removal, automatic-cleanup
    claiming, deferred cleanup, and other relevant
    singleton-infrastructure lifecycle operations.

### Changed

-   Extended the existing Observer architecture from individual `Thread`
    and `PrecisionThread` instances to process-wide singleton
    infrastructure.
-   Kept the notification layer synchronous and independent of ESPressio
    Event so Event bridges can remain opt-in.

## [3.0.0] - 2026-08-18

### Added

-   Added generic `PrecisionThread<TTime, Traits>` timing representation
    support.
-   Added support for ordinary ESPressio Units, Serializable Units, and
    future Timing-compatible representations.
-   Added/updated examples for basic Threads, lifecycle observation,
    automatic garbage collection, Precision Threads, and Serializable
    Precision Threads.

### Changed

-   Migrated precision scheduling to ESPressio Timing 2.x.
-   Separated internal nanosecond scheduling from public Unit
    representation.
-   Updated the dependency baseline to Timing 2.x and Observable 3.x.

### Fixed

-   Corrected integration issues found while validating the 3.0 codebase
    against the newest ESPressio dependencies.

## [2.0.0] - 2026-08-13

### Changed

-   Changed Thread and PrecisionThread Observer registration APIs to
    return owning `Observable::ObserverHandlePtr` handles.
-   Adopted ESPressio Observable 3.0 ownership semantics.

### Fixed

-   Removed raw Observer-handle ownership/leak ambiguity.
-   Released retained PrecisionThread sample storage when the sampling
    window is changed or disabled.
-   Clarified process-lifetime Thread infrastructure ownership.

## [1.4.1]

### Fixed

-   Maintenance corrections to the 1.4 series.

## [1.3.0]

### Added

-   Expanded the Thread/PrecisionThread feature set and lifecycle
    infrastructure.

## [1.2.0]

### Added

-   Continued development of precision-threading and
    lifecycle-management capabilities.

## [1.1.0]

### Added

-   Extended the initial Thread abstraction with additional
    lifecycle/scheduling functionality.

## [1.0.0]

### Added

-   Initial public release of ESPressio Threads and its object-oriented
    ESP32/FreeRTOS Thread foundation.

> Releases 1.0.0 through 1.4.1 are preserved here as terse historical
> entries because their detailed release-note text is not fully exposed
> in the current reconstructed source set.
