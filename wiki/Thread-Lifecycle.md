# Thread Lifecycle

A Thread has an explicit managed lifecycle rather than exposing a raw scheduler task.

The lifecycle coordinates initialization, start gating, persistent loop execution, termination, task-exit synchronization, observer notification, registry state and optional automatic cleanup.

## Initialization

Initialization prepares the worker and its execution resources according to the configured stack, priority and processor-affinity request. Required shared lifecycle infrastructure is made available before the worker can start.

## Loop execution

`OnLoop()` represents one iteration of the long-lived worker. The derived type determines whether an iteration blocks, sleeps, waits for work, or immediately repeats.

## Termination

Termination is coordinated so the worker does not delete its own native execution resources while still executing on them. Cleanup occurs from the appropriate independent lifecycle context.

## Reinitialization

Creation-time configuration is changed only after the current execution has been fully terminated/shut down.

## User callbacks

Lifecycle callbacks and observers are user code. Internal locks must not be held across those calls unless the documented contract explicitly requires it.