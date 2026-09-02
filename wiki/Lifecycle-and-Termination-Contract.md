# Lifecycle and Termination Contract

Termination is one of the most concurrency-sensitive parts of Threads and must preserve strict ownership/order guarantees.

## Required sequence

A terminating worker records/queues its termination state, stops executing its normal loop, and transfers native-execution teardown to an independent lifecycle context. Termination callbacks and task-exit observers are delivered before automatic object cleanup eligibility is finally evaluated.

## Dispatcher availability

The termination dispatcher may be initialized lazily, but the first normal Thread cannot start unless required termination infrastructure is available. Lazy allocation must never make lifecycle correctness depend on incidental startup order.

## Cross-core publication

Shared queue/task/synchronization infrastructure must be published in an order safe for a newly created worker becoming runnable immediately on another processor.

## Cleanup handoff

After the manager claims automatic cleanup, code must assume the Thread object can be destroyed and avoid any later raw-pointer dereference.

## Reentrancy

User callbacks may modify lifecycle policy such as automatic cleanup. Preserve the documented callback-before-cleanup ordering and never invoke them while holding internal locks that would make valid lifecycle calls deadlock.