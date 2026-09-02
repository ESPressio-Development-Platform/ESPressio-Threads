# Manager Registry Contract

`ThreadManager` is the authority for registered Thread identity, iteration and automatic cleanup.

## Lock-boundary rule

Manager mutexes protect registry bookkeeping, but virtual/user callbacks must not execute while those locks are held. Use snapshots/claims to carry only the information required beyond the lock boundary.

## Iteration safety

Track active manager iterations. If cleanup would invalidate an active iteration, defer it and complete pending cleanup when the final iteration guard exits.

## Cleanup claims

Automatic cleanup must be atomically claimed exactly once. Explicit-release Threads remain ineligible until their lifecycle policy permits reclamation.

## Memory policy

Variable-size ESPressio-owned registry, snapshot and cleanup-claim storage should use the configured `ExternalPreferred` System memory policy where safe. Release snapshots as soon as the lock-boundary purpose is complete.

## Identity

Preserve deterministic ID registration and explicit errors for null/invalid registration, duplicates and ID-space exhaustion.