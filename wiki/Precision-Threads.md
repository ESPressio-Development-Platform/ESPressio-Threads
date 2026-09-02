# Precision Threads

Precision Threads extend the long-lived Thread abstraction with timing-driven execution against ESPressio Timing rather than an ad-hoc delay loop.

Use them when a worker iteration should be scheduled according to a defined time/frequency contract and when accumulated timing error, clock discipline and deadline semantics matter.

## Timing integration

Precision scheduling consumes ESPressio Timing's clock abstractions and public time representations. The worker remains a Thread; Timing owns the clock/deadline semantics.

## Public representation

Precision Thread traits can use ordinary ESPressio Units time/frequency types. Serializable-compatible variants remain optional and should not make Threads itself dependent on ESPressio Serializable for ordinary use.

## Clock discipline

When Precision Threads depend on a disciplined System Clock, prefer monotonic synchronization modes such as Timing's `SlewOnly`. Hard clock steps can change the relationship between a previously calculated deadline and the public timeline.

## Scheduling context

Precision scheduling must use portable System/Timing wait/wake capabilities rather than exposing target-native task-notification primitives to consumers.