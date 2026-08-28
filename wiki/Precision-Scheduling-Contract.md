# Precision Scheduling Contract

Precision Thread scheduling should remain a portable Timing-domain deadline mechanism rather than a target-specific delay loop.

## Clock source

Use ESPressio Timing clock semantics and the selected public time/frequency representation. Convert to raw scheduling deadlines through the established traits/contracts rather than repeatedly using native scheduler tick math in application-facing code.

## Wait/wake abstraction

Scheduling waits and wake signals should consume portable System/Task capabilities where the architecture provides them. Native notification/semaphore details belong below the abstraction boundary.

## Monotonic assumptions

Document which clock timeline drives deadlines. If a disciplined System Clock is used, monotonic slew modes are preferred while workers are active. Explicit clock steps can invalidate assumptions about previously calculated deadlines.

## Overrun behaviour

Define what happens when an iteration misses its intended deadline: skip, catch up, or schedule from the next epoch according to the Precision Thread contract. Do not hide repeated overruns behind accumulating drift.

## Testing

Use deterministic/fake clock sources to test interval accuracy, accumulated drift, wakeups, overruns, clock corrections and stop/termination while waiting.