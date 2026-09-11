# Threads coordinated dependency boundary

All repository references below use `primitives_redesign`.

| Repository | Ownership consumed by Threads |
|---|---|
| System | Execution provider, monotonic clock, synchronization |
| Task | Queue-free physical execution configuration and TaskRuntime |
| Timing | Monotonic numeric timing utilities |
| Units | Platform-wide unit representation boundary |

Event, State and Command depend on Threads and own their respective capabilities.
Threads has no dependency on these families, Primitive, Radio, Mesh or Adapters.
Observable and Serializable are not direct Threads dependencies. Concrete native
execution lives in the System provider, supplied by ESP32 on that platform.
