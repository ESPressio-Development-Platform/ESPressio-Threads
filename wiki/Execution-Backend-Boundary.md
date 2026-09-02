# Execution Backend Boundary

Threads should consume ESPressio Task/System runtime abstractions instead of exposing or duplicating target scheduler ownership.

## Portable layer

Thread code expresses worker configuration, lifecycle, wait/wake requirements, current execution identity, processor capability and stack telemetry through ESPressio concepts.

## Target layer

The target platform/provider stack maps those semantics onto FreeRTOS or another scheduler/runtime.

## Extension rule

A new MCU/RTOS should become usable by satisfying System/Task provider contracts. Do not add `#ifdef <target>` branches throughout the Thread lifecycle merely to reach a native task handle or scheduler function.

## Native memory

Execution stacks/control structures remain subject to the target runtime's memory requirements. Provider implementations—not Threads bookkeeping optimisations—decide how that native memory can be allocated safely.

## Guardrails

Tests/CI should reject accidental reintroduction of direct native task creation/deletion into paths already migrated to the portable backend abstraction.