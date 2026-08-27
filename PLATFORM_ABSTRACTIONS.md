# Platform Abstractions Audit Trail

This file records changes made during the platform-abstraction tranche tracked by issue #79.

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

## Remaining work in this repository

- Update README compatibility/platform sections to describe Threads as a portable lifecycle layer over System/Task rather than an ESP32/FreeRTOS-specific library.
- Run the repository CI suite against the coordinated System/Task working branches and correct any regressions.

## Boundary

ESPressio-Threads owns long-lived thread lifecycle, precision scheduling and thread-manager semantics. ESPressio-System owns primitive execution/synchronization/queue capabilities; ESPressio-Task provides discrete task runtime semantics over those primitives; ESPressio-ESP32 supplies the FreeRTOS implementation for ESP32 targets.
