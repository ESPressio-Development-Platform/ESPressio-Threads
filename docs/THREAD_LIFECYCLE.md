# ESPressio Thread Task-Lifecycle Contract

This document describes the corrected task-lifecycle contract used by the current ESPressio Threads working branch after critical issue #67 and the Task-runtime migration tracked by #72.

## Ownership

An `ESPressio::Threads::Thread` owns the FreeRTOS task created for that Thread.

Applications interact with task lifetime through the ESPressio lifecycle API:

```text
Initialize() / Start()
Pause()
Terminate()
Shutdown()
ThreadManager / automatic ReleaseOnTerminate reclamation
```

Calling `vTaskDelete()` directly on the underlying task of an ESPressio `Thread` is an unsupported lifecycle bypass. Direct deletion bypasses the object state machine, Observer/callback contract, termination dispatcher, shutdown synchronization, and manager ownership rules.

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
    -> if ReleaseOnTerminate, asks ThreadManager to reclaim eligible objects

ThreadManager
    -> atomically claims automatic cleanup ownership
    -> unregisters eligible terminated Threads
    -> deletes them outside manager locks
    -> defers cleanup automatically if a manager iteration is active
```

The worker deliberately does not self-delete. Keeping deletion in the termination dispatcher guarantees that no `Thread` object can be destroyed while code is still executing on that object's own FreeRTOS task stack.

There is no dedicated garbage-collector task. Earlier implementations used a separate 2000-byte infrastructure task whose only ultimate operation was `ThreadManager::CleanUpWithResult()`. That worker, its semaphore, observer surface, and fallback mode were redundant once termination dispatch already provided a safe independent execution context. Automatic reclamation now moves directly from the termination dispatcher to `ThreadManager`.

## Release policies

`ThreadReleasePolicy::ExplicitRelease` remains owner-controlled. Termination removes the native task and completes lifecycle notifications, but manager cleanup does not claim or delete the C++ object.

`ThreadReleasePolicy::ReleaseOnTerminate` remains manager-controlled after termination. Once the dispatcher has deleted the native task and completed all callbacks that may dereference the Thread, `ThreadManager::CleanUpWithResult()` may claim, unregister, and delete the object.

The historical `Thread::GarbageCollect()` method is retained only as a source-compatibility shim. It no longer creates or signals a garbage-collector task; it delegates directly to `ThreadManager` cleanup for eligible `FreeOnTerminate` objects.

## Cleanup during manager iteration

Automatic reclamation does not require a second infrastructure worker for deferral.

`ThreadManager` tracks active iterations. If cleanup is requested while `ForEachThread()`, `WithThread()`, initialization, termination, or another guarded manager traversal is active, `CleanUpWithResult()` records `_cleanupPending` and returns without deleting objects. The final `IterationGuard` to exit performs the deferred cleanup from that safe context.

This preserves the existing no-delete-during-iteration contract while avoiding an additional permanent task stack.

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

The same dispatcher/manager reclamation contract is then used, but there is no worker task left for the dispatcher to delete.

## Idempotence

Task-exit finalization is guarded by an atomic once-only claim. Concurrent lifecycle activity must not enqueue duplicate termination records or perform duplicate task-exit bookkeeping.

Automatic reclamation is independently protected by `TryClaimAutomaticCleanup()`, so a Thread can only be won once by manager-driven deletion.

`Shutdown()` may race with a worker observing `Terminating`; it waits until termination dispatch has deleted the worker task and completed its final access to the Thread object.

## Callback milestones

`OnThreadTerminated` / the `OnTerminate` state callback represents the logical transition to the `Terminated` state.

`OnTerminated` and `IThreadObserver::OnThreadTaskExited()` are later milestones delivered by the termination dispatcher after the underlying FreeRTOS worker has been deleted.

Use the latter milestone when cleanup depends on the worker no longer executing.

After those callbacks return, a `ReleaseOnTerminate` object may be reclaimed immediately. Code must therefore not retain or dereference a raw pointer to an automatically released Thread after its task-exit milestone.

## Dispatcher pointer-lifetime rule

The termination dispatcher stores a value snapshot alongside the raw Thread pointer. It may dereference the pointer only through `_dispatchTermination()`.

After `_dispatchTermination()` returns, manager cleanup may delete the object. All subsequent dispatcher notifications therefore use the captured `ThreadManagerThreadSnapshot` and must not dereference `record.ThreadPointer` again.

## Queue behaviour

Before #67, termination dispatch originated from a FreeRTOS task-deletion callback, so queue submission had to be non-blocking.

Termination dispatch now originates from normal ESPressio lifecycle code. Queue submission may wait for capacity, preventing a normally exiting task from being stranded because the dispatcher queue is temporarily full.

## Resource profile

The permanent Threads lifecycle infrastructure now requires only the termination dispatcher task. Removing the dedicated garbage collector eliminates its 2000-byte stack plus its FreeRTOS task-control allocation, binary semaphore, and observer/infrastructure state.

This matters especially on ESP32 configurations where Wi-Fi and ESP-NOW compete for scarce internal-capable DRAM even when external PSRAM remains plentiful.

## Critical 3.1.7 release replacement

The originally published Threads 3.1.7 implementation is invalidated because it wrote an ESPressio `Thread*` into a FreeRTOS TLS slot reserved by ESP-IDF pthread internals.

For this critical correction only, normal semantic-versioning rules were intentionally overridden. The current #72 working branch is later development work and does not imply a version or release change by itself.

Ordinary callers using the ESPressio lifecycle API remain source-compatible. Code which directly deletes an ESPressio-owned task with `vTaskDelete()` must migrate to `Terminate()` or `Shutdown()` as appropriate.
