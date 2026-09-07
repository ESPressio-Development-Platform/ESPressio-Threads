# Platform Abstractions Audit Trail

This file records changes made during the platform-abstraction tranche tracked by issue #79.

## 2026-09-07

- Corrected `PrecisionThread` so its default scheduling domain is the shared `Timing::MonotonicClock`, not the settable/distributed `Timing::SystemClock`.
- Narrowed `PrecisionThread::ClockType` from `Timing::ISystemClock<TTime>` to `Timing::IClock<TTime>`; explicit custom clocks, including an explicitly requested System Clock, remain injectable.
- The default `Timing::MonotonicClock` is a non-owning facade over Timing's process-wide `HighResolutionTimeSource`; it therefore does not allocate one hardware timer per Thread.
- `MonotonicClock` resolves the shared high-resolution source lazily on first read so global/static `PrecisionThread` construction does not pre-empt platform provider installation during `setup()`.
- On ESP32, `ESP32Platform::InstallSystemProviders()` installs a GPTimer-backed high-resolution counter provider. Once installed before the first Timing source read, the shared Timing high-resolution source owns one GPTimer counter consumed by every default `PrecisionThread`.
- System Clock hard steps, slews and synchronization corrections alter only the distributed System Clock facade. They do not change the raw monotonic source used for PrecisionThread deadlines, deltas, skipped-iteration accounting or wake cadence.
- Updated automated dependency pins so the Threads working branch compiles against the Timing Mesh-propagation branch carrying the monotonic clock API.

## 2026-08-27

- Replaced termination-dispatcher native FreeRTOS queue/task handles with System queue and Task/System execution handles.
- Reworked core `Thread` lifecycle state to use portable execution handles.
- Replaced the FreeRTOS task-exit semaphore with a System binary signal.
- Replaced task-notification startup gating with a System binary signal.
- Replaced direct task delay/yield/suspend calls with `TaskRuntime`, which delegates to System execution.
- Reworked `PrecisionThread` scheduler wake/wait signalling to use System synchronization rather than a FreeRTOS semaphore.
- Replaced native tick-duration scheduler waiting with portable millisecond timeout requests at the System signal boundary.
- Added a portable processor-count capability to System execution and migrated `ThreadManager` from `portNUM_PROCESSORS` / `configNUMBER_OF_CORES` discovery to `System::Execution::Provider().ProcessorCount()`.
- Verified the active termination-dispatcher headers no longer include native FreeRTOS headers; the historical garbage-collector source is not present on the working branch.
- Updated README platform architecture guidance to describe Threads as the portable lifecycle/scheduling layer over ESPressio-System and ESPressio-Task, with ESPressio-ESP32 supplying the FreeRTOS implementation on ESP32. Published 3.1.7 material is explicitly retained as release-history documentation rather than current working-branch architecture.
- Validated the abstraction branch against the coordinated System/Task working branches. Host/runtime boundary guards, ESP32 compilation, Task-runtime compilation, TLS ownership, and WiFi/thread coexistence checks completed successfully before the documentation-only completion commits.

## Boundary

ESPressio-Threads owns long-lived thread lifecycle, precision scheduling and thread-manager semantics. ESPressio-System owns primitive execution/synchronization/queue capabilities; ESPressio-Task provides discrete task runtime semantics over those primitives; ESPressio-Timing owns the shared monotonic/high-resolution scheduling clock abstraction; ESPressio-ESP32 supplies the GPTimer/FreeRTOS implementations for ESP32 targets.
