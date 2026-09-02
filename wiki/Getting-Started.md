# Getting Started

Derive a long-lived worker from `Thread` and implement its lifecycle hooks:

```cpp
#include <ESPressio_Thread.hpp>

using namespace ESPressio::Threads;

class SensorWorker final : public Thread {
protected:
    void OnInitialization() override {
        // prepare worker-owned state
    }

    void OnLoop() override {
        // one worker iteration
    }
};
```

Create and initialize the worker during application startup:

```cpp
SensorWorker worker;

void setup() {
    worker.Initialize();
}
```

## What a Thread represents

A Thread is a lifecycle-managed persistent worker. It is appropriate when the component owns continuing behaviour rather than a collection of independent queued work items.

## Platform abstraction

The 1.0.0 architecture sits above ESPressio Task/System runtime abstractions. Consumer code should not depend on native task handles, queues, processor-count macros or target scheduler types.

## Configuration timing

Stack size, priority and requested processor/core placement are creation-time concerns. Configure them before initialization; terminate/shut down before changing settings for the next initialization.

## Next steps

- [Thread Lifecycle](Thread-Lifecycle)
- [Thread Configuration](Thread-Configuration)
- [Task versus Threads](Task-versus-Threads)
- [Resource and Memory Behaviour](Resource-and-Memory-Behaviour)