# ESPressio Threads Optimisation Log

This file records the current resource-optimisation round chronologically. Version numbers are intentionally unchanged during this round.

Rollback anchor: `rollback/optimisations-pre-20260825` -> `20f8579142fa6a4e0da25643748adf3ec25f33d8`.

## 2026-08-25 — Optimisation round opened (#69)

### Context
Full-stack ESP32 hardware validation exposed severe internal-RAM pressure when Threads, Event, WiFi, ESP-NOW, serialization and device hardware coexist. Threads is a systemic target because every `Thread` contributes task-stack reservation and repeated synchronization/lifecycle infrastructure.

### Planned low-risk/high-gain work
- Measure and reduce over-conservative task-stack defaults only where a safe margin can be demonstrated.
- Consolidate scalar configuration/state synchronization where equivalent concurrency guarantees can be preserved with atomics or one compact lock.
- Remove unnecessary heap/shared ownership for per-Thread lifecycle observable state where object lifetime already provides ownership.
- Preserve initialization, lifecycle, observer, termination-dispatch, garbage-collection and callback reentrancy semantics.
- Add regression/stress coverage before accepting concurrency-sensitive changes.

### Safety / rollback
No optimisation is accepted solely for memory savings. Changes must preserve behavioral and thread-safety guarantees. The rollback branch above remains an immutable pre-optimisation reference.
