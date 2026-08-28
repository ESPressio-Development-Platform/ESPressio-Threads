# Resource and Memory Behaviour

Threads 1.0.0 deliberately distinguishes ESPressio-owned bookkeeping from runtime/RTOS-owned execution memory.

## External-preferred bookkeeping

Where safe and supported by ESPressio System, variable-size ThreadManager registry/snapshot data and per-Thread lifecycle Observable allocation prefer external memory. Snapshot lifetime is also reduced as soon as cleanup claims are resolved.

## Internal/native execution memory

Task stacks, native queue/semaphore/control structures and other runtime memory that requires native/internal capabilities remain owned by the runtime/provider and are **not** moved to external RAM merely to reduce DRAM pressure.

## Bounded termination infrastructure

The termination-dispatch queue is bounded and right-sized rather than reserving capacity for the theoretical maximum Thread ID space.

## No standalone garbage-collector worker

Automatic reclamation is coordinated by the existing termination dispatcher plus ThreadManager, avoiding an additional task stack/control structure.

## Measure before reducing stacks

Stack defaults should be reduced only from representative high-water evidence with retained safety margin. A successful idle/short test is not sufficient evidence for production stack sizing.

## Observable ownership

Notification-capable Observable helpers retain the shared-ownership model required by ESPressio Observable even when direct ownership would appear to save allocations.