# ESPressio Thread Task-Lifecycle Contract

This document describes the corrected task-lifecycle contract used by ESPressio Threads 3.1.7 after critical issue #67.

## Ownership

An `ESPressio::Threads::Thread` owns the FreeRTOS task created for that Thread.

Applications interact with task lifetime through the ESPressio lifecycle API:

```text
Initialize() / Start()
Pause()
Terminate()
Shutdown()
ThreadManager / automatic garbage collection
```

Calling `vTaskDelete()` directly on the underlying task of an ESPressio `Thread` is an unsupported lifecycle bypass. Direct deletion bypasses the object state machine, Observer/callback contract, termination dispatcher, shutdown synchronization, and garbage-collection ownership rules.

## No FreeRTOS TLS ownership

ESPressio Threads does not reserve, write, inspect, or own generic FreeRTOS thread-local-storage pointer slots.

In particular, ESPressio must not use TLS index 0. ESP-IDF uses that slot for pthread internals, and overwriting it can corrupt APIs which obtain per-thread state through pthread, including Wi-Fi driver operations.

The former `ESPRESSIO_THREAD_TLS_INDEX` implementation mechanism has been removed.

## Normal task exit

A normal ESPressio task exits through an explicit ESPressio-owned finalization path:

```text
Thread task
    -> OnLoop() completes / Terminate() observed / exception contained
    -> state reaches Terminated
    -> task-exit finalization claimed exactly once
    -> termination dispatch queued
    -> worker task suspends

Termination dispatcher
    -> deletes the suspended FreeRTOS worker task
    -> invokes OnTerminated callback
    -> notifies OnThreadTaskExited observers
    -> releases Shutdown() waiters
    -> allows automatic garbage collection
```

The worker deliberately does not self-delete. Keeping deletion in the termination dispatcher guarantees that no `Thread` object can be destroyed while code is still executing on that object's own FreeRTOS task stack.

## Initialization failure

The worker task is created gated and does not execute `OnLoop()` until initialization succeeds.

If initialization fails or the Thread is terminated during initialization:

```text
initializing task
    -> transition Thread to Terminated
    -> delete still-gated worker task
    -> claim task-exit finalization
    -> queue termination dispatch
```

The same deferred callback/Observer/GC contract is then used, but there is no worker task left for the dispatcher to delete.

## Idempotence

Task-exit finalization is guarded by an atomic once-only claim. Concurrent lifecycle activity must not enqueue duplicate termination records or perform duplicate task-exit bookkeeping.

`Shutdown()` may race with a worker observing `Terminating`; it waits until termination dispatch has deleted the worker task and completed its final access to the Thread object.

## Callback milestones

`OnThreadTerminated` / the `OnTerminate` state callback represents the logical transition to the `Terminated` state.

`OnTerminated` and `IThreadObserver::OnThreadTaskExited()` are later milestones delivered by the termination dispatcher after the underlying FreeRTOS worker has been deleted.

Use the latter milestone when cleanup depends on the worker no longer executing.

## Queue behaviour

Before #67, termination dispatch originated from a FreeRTOS task-deletion callback, so queue submission had to be non-blocking.

Termination dispatch now originates from normal ESPressio lifecycle code. Queue submission may wait for capacity, preventing a normally exiting task from being stranded because the dispatcher queue is temporarily full.

## Critical 3.1.7 release replacement

The originally published Threads 3.1.7 implementation is invalidated because it wrote an ESPressio `Thread*` into a FreeRTOS TLS slot reserved by ESP-IDF pthread internals.

For this critical correction only, normal semantic-versioning rules are intentionally overridden. The version remains `3.1.7`, and the existing release/tag is intended to be moved/reissued to the corrected commit rather than cascading another dependency-version release through the platform.

Ordinary callers using the ESPressio lifecycle API remain source-compatible. Code which directly deletes an ESPressio-owned task with `vTaskDelete()` must migrate to `Terminate()` or `Shutdown()` as appropriate.
