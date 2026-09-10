# ESPressio Dependency Chart — Current Released Generation

![ESPressio Library Dependency Chart](ESPRESSIO_DEPENDENCY_CHART.svg)

## Released generation

```text
Observable
Serializable
Units
Timing
Threads
Event
Command
Security
Persistence
Sockets
ESP-Now
WiFi
Serial
```

## Threads dependency position

```text
Threads
    -> Timing main
    -> Observable main

Threads Serializable integration
    - - -> Serializable representations transitively through Units main
```

Threads deliberately does **not** declare Serializable as a core package dependency. `ESPressio_PrecisionThread.hpp` remains serialization-agnostic; Serializable time/frequency traits are opt-in.

## Completed cascade

```text
Serializable
    -> Units
    -> Timing
    -> Threads
    -> Event
    -> Command / Security
    -> Persistence / Sockets / ESP-Now
    -> WiFi
    -> Serial
```

Event may consume Threads; Threads must not depend on Event. Serial remains terminal/downstream. ESPressio Tree remains standalone.
