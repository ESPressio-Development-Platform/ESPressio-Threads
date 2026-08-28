# Testing Thread Extensions

Threads tests must exercise lifecycle and concurrency semantics under realistic scheduling, not only compile success.

## Lifecycle

Cover initialization/start gating, repeated loop execution, self/external termination, shutdown waits, reinitialization and configuration immutability while an execution exists.

## Termination/cleanup

Stress bounded termination queue backpressure, dispatcher startup, cross-core publication ordering, callback ordering, automatic versus explicit release, manager cleanup claims and deferred cleanup during active iteration.

## Manager

Test registration limits, duplicate/custom IDs, snapshot/iteration lock boundaries and concurrent register/terminate/iterate paths.

## Observers

Exercise actual notification paths so shared-ownership assumptions are tested rather than merely constructed. Test callback reentrancy and exception containment according to the public contract.

## Precision scheduling

Use deterministic clocks for cadence, overrun, wake/stop and disciplined-clock behaviour.

## Resources

Collect stack high-water and heap/internal-memory measurements under representative workload. Accept footprint reductions only when lifecycle/thread-safety behaviour remains unchanged and a safety margin remains.