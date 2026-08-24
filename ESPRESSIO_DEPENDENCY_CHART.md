# ESPressio Dependency Chart — Current Released Generation

![ESPressio Library Dependency Chart](ESPRESSIO_DEPENDENCY_CHART.svg)

## Released generation

```text
Observable    3.0.2
Serializable  0.11.3
Units         0.2.7
Timing        2.2.8
Threads       3.1.7
Event         6.0.3
Command       1.0.3
Security      0.4.2
Persistence   0.3.2
Sockets       0.7.3
ESP-Now       0.8.3
WiFi          0.2.0
Serial        0.8.1
```

## Threads dependency position

```text
Threads 3.1.7
    -> Timing >= 2.2.8 < 3.0.0
    -> Observable >= 3.0.2 < 4.0.0

Threads Serializable integration
    - - -> Serializable representations transitively through Units 0.2.7
```

Threads deliberately does **not** declare Serializable as a core package dependency. `ESPressio_PrecisionThread.hpp` remains serialization-agnostic; Serializable time/frequency traits are opt-in.

## Completed cascade

```text
Serializable 0.11.3
    -> Units 0.2.7
    -> Timing 2.2.8
    -> Threads 3.1.7
    -> Event 6.0.3
    -> Command 1.0.3 / Security 0.4.2
    -> Persistence 0.3.2 / Sockets 0.7.3 / ESP-Now 0.8.3
    -> WiFi 0.2.0
    -> Serial 0.8.1
```

Event may consume Threads; Threads must not depend on Event. Serial remains terminal/downstream. ESPressio Tree remains standalone.
