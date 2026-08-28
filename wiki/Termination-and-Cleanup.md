# Termination and Cleanup

Thread termination is coordinated so an executing worker is not responsible for destroying the native execution context on which it is still running.

## Termination dispatcher

A terminating Thread queues one termination-dispatch record. The dispatcher owns the independent context required to complete native worker teardown, publish termination/task-exit notifications, release shutdown waiters and then request manager cleanup.

The default termination queue is bounded at 32 entries in the 1.0.0 optimisation baseline and remains compile-time configurable. Saturation applies backpressure rather than silently dropping a termination request.

## No dedicated garbage-collector worker

The historical dedicated garbage-collector task has been removed. After termination dispatch completes, the dispatcher calls `ThreadManager::CleanUpWithResult()` directly.

`ThreadManager` remains the authority for automatic-cleanup eligibility, claims, registry removal, iteration deferral and C++ object deletion.

## Callback ordering

Cleanup is evaluated after termination callbacks so an `OnTerminated` observer/callback can still change `FreeOnTerminate` before manager eligibility is evaluated.

Once manager cleanup begins, termination code must not dereference a raw Thread pointer that may have been deleted.

## Explicit release

Threads configured for explicit release remain outside automatic cleanup until ownership is deliberately relinquished.