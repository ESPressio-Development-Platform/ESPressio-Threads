# Platform Abstractions Audit Trail

This file records changes made during the platform-abstraction tranche tracked by issue #79.

## 2026-08-27

- Replaced termination-dispatcher native FreeRTOS queue/task handles with System queue and Task/System execution handles.
- Reworked core `Thread` lifecycle state to use portable execution handles.
- Replaced the FreeRTOS task-exit semaphore with a System binary signal.
- Replaced task-notification startup gating with a System binary signal.
- Replaced direct task delay/yield/suspend calls with `TaskRuntime`, which now delegates to System execution.
- Reworked `PrecisionThread` scheduler wake/wait signalling to use System synchronization rather than a FreeRTOS semaphore.
- Replaced native tick-duration scheduler waiting with portable millisecond timeout requests at the System signal boundary.
- Added a portable processor-count capability to System execution for removal of RTOS core-count macro discovery from thread allocation logic.

## Remaining work in this repository

- Replace the `ThreadManager` compile-time FreeRTOS core-count discovery with `System::Execution::Provider().ProcessorCount()`.
- Complete a source-wide verification for any residual direct FreeRTOS include/type/call after the core lifecycle migration.
- Update README compatibility/platform sections once the source-wide verification is complete.
- Run the repository CI suite against the coordinated System/Task working branches and correct any regressions.

## Boundary

ESPressio-Threads owns long-lived thread lifecycle, precision scheduling and thread-manager semantics. ESPressio-System owns primitive execution/synchronization/queue capabilities; ESPressio-Task provides discrete task runtime semantics over those primitives; ESPressio-ESP32 supplies the FreeRTOS implementation.
