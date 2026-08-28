# Task versus Threads

Use **ESPressio Task** for discrete asynchronous work and **ESPressio Threads** for long-lived autonomous worker lifecycle.

## Prefer Task / TaskExecutor when

- work arrives as independent items;
- a bounded queue/backpressure policy is central;
- one persistent executor can process many messages;
- the domain does not need a long-lived worker object with its own lifecycle.

## Prefer Threads when

- the component itself is a continuing worker;
- initialization/start/loop/termination are meaningful lifecycle states;
- persistent configuration, manager registration and lifecycle observers matter;
- precision periodic/deadline scheduling is part of the worker abstraction.

## Relationship

Threads can consume Task/System runtime facilities internally. That does not make the two public abstractions interchangeable: Task models **work**, Threads models **workers**.