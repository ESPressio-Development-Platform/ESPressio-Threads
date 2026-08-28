# Thread Safety Utilities

Threads includes synchronization/value wrappers such as `Mutex<T>` and `ReadWriteMutex<T>` for state that must be protected across concurrent worker contexts.

## Read/write semantics

Use shared/read access when multiple readers can safely coexist and exclusive/write access when mutation requires exclusion.

## Change callbacks and comparators

The 1.0.0 optimisation baseline stores optional custom comparator/on-change callback state lazily. Normal wrappers that use neither facility avoid permanently carrying full `std::function` objects.

Callbacks are captured while protected state is stable and invoked after the internal lock is released so user code does not execute beneath the wrapper lock.

## Derived component responsibility

Threads can provide synchronization primitives, but a derived worker still owns the correctness of its shared-state design. Choose lock scope/order deliberately and avoid recursive cross-component lock dependencies.

## Atomics versus locks

Use atomics for genuinely independent scalar state where their memory-order semantics fully satisfy the invariant. Do not replace a lock protecting a multi-field invariant merely to reduce object size.