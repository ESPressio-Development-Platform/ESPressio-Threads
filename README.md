# ESPressio Threads
Threading Components of the ESPressio Development Platform.

Light-weight and easy-to-use Threading for your Microcontroller development work.

## Current Source Version
This source tree is version **3.1.7**.

Refer to the GitHub Releases page for the latest published release/tag.

Current required ESPressio baselines:

```text
Timing >= 2.2.8 < 3.0.0
Observable >= 3.0.2 < 4.0.0
```

Timing 2.2.8 carries Units 0.2.7 downstream. Threads itself remains independent of ESPressio Serializable; applications that opt into `ESPressio_PrecisionThread_Serializable.hpp` use Serializable Unit time/frequency representations and the 3.1.7 release is validated against Serializable 0.11.3.

## Compatibility

ESPressio Threads `3.1.7` targets the **ESP32 family under Arduino-ESP32**. This includes classic ESP32 and current single- and multi-core variants such as ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, and ESP32-P4 when supported by the installed Arduino-ESP32/framework version. Single-core devices use CPU 0; multiple hardware cores are not required.

The implementation directly uses ESP-IDF FreeRTOS task, queue, semaphore, and task-local-storage APIs through Arduino-ESP32. The source architecture remains intentionally close to ESP-IDF and the repository retains its CMake/component files, but the `3.1.7` PlatformIO package does **not currently advertise pure ESP-IDF framework support** because the published ESPressio Timing/Units dependency chain does not yet advertise the same framework compatibility.

The library is not compatible with ESP8266 or non-ESP32 families such as AVR, SAMD, RP2040, STM32, or Renesas merely because another FreeRTOS port is available there.

Compatibility should always be verified against the exact Arduino-ESP32 version and ESP32 target used by the consuming application.

## ESPressio Development Platform
The **ESPressio** Development Platform is a collection of discrete (sometimes intra-connected) Component Libraries developed with a particular development ethos in mind.

The key objectives of the ESPressio Development Platform are:
- **Light-weight** - The Components should always strive to optimize memory consumption and operational overhead as much as possible, but not to the detriment of...
- **Ease of Use** - Many of our components serve as Developer-Friendly Abstractions of existing procedural code libraries.
- **Object-Oriented** - A `type` for everything, and everything in a `type`!
- **SOLID**:
- -  > **S**ingle Responsibility Principle (SRP)
    Break your code into smaller, focused components.
- - > **O**pen/Closed Principle (OCP)
    Be open for extension but closed for modification.
- - > **L**iskov Substitution Principle (LSP)
    Be substitutable for the base type without altering correctness.
- - > **I**nterface Segregation Principle (ISP)
    Break interfaces into specific, client-focused ones.
- - > **D**ependency Inversion Principle (DIP)
    Be dependent on abstractions, not concretions.

To the maximum extent possible within the limitations/restrictons/constraints of the C++ langauge, the Arduino platform, and Microcontroller Programming itself, all Component Libraries of the **ESPressio** Development Platform must strive to honour the **SOLID** principles.

## License
ESPressio (and its component libraries, including this one) are subject to the *Apache License 2.0*
Please see the [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE) accompanying this library for full details.

## Namespace
Every type/variable/constant/etc. related to *ESPressio* Threads are located within the `Threads` sub-namespace of the `ESPressio` parent namespace.

The namespace provides the following (*click on any declaration to navigate to more info*):
- [`ESPressio::Threads::IThread`](#ithread)
- `ESPressio::Threads::IThreadObserver`
- [`ESPressio::Threads::Thread`](#thread)
- `ESPressio::Threads::PrecisionThreadTraits<TTime>`
- `ESPressio::Threads::IPrecisionThreadObserver<TTime, TRepresentationTraits>`
- `ESPressio::Threads::PrecisionThread<TTime, TRepresentationTraits>`
- [`ESPressio::Threads::Manager`](#threadmanager)
- [`ESPressio::Threads::GarbageCollector`](#garbagecollector)
- [`ESPressio::Threads::IThreadSafe`](#ithreadsafe)
- [`ESPressio::Threads::Mutex`](#mutex)
- [`ESPressio::Threads::ReadWriteMutex`](#readwritemutex)

## Platformio.ini
You can quickly and easily add this library to your project in PlatformIO by simply including the following in your `platformio.ini` file:

```ini
lib_deps =
    espressio-development-platform/ESPressio-Threads@^3.1.7
```

Alternatively, if you want to use the bleeding-edge (effectively "Developer Integration Testing" or "DIT") sources, you can instead use:

```ini
lib_deps = 
	https://github.com/ESPressio-Development-Platform/ESPressio-Threads.git
```
Please note that this will use the very latest commits pushed into the repository, so volatility is possible.

## Understanding Threads
Threads enable us to perform concurrent and/or parallel processing on our microcontroller devices.
In the case of multi-core microcontrollers, such as the ESP32, we can achieve true concurrent execution by using the components provided here in the *ESPressio* Thread Library.

By default, when an instance of a [`Thread`](#thread) descendant is created, presuming that you do not modify by calling `SetCoreID()` prior to initializing the instance, the Thread [`Manager`](#threadmanager) will automatically allocate the Thread to the next available CPU Core. Single-core targets always use CPU 0.

For example, by default, your first Thread Instance will occupy *CPU 0*, your second will occupy *CPU 1*, your third will co-occupy *CPU 0*.

However, as hinted previously (and as you'll see later in this document) you can very easily define explicitly which CPU Core you want your `Thread` to run on.

Core ID, stack size, and priority are creation-time settings. Their setters are ignored while a FreeRTOS task exists, ensuring the corresponding getters cannot report settings different from the running task. Terminate or shut down the Thread before changing these settings for its next initialization.

Now, when your Microcontroller doesn't have multiple CPU Cores, or when you have multiple threads co-tenanting the same CPU Cores, Threads will operate on the princpals of *Time Slicing*. This is where `Thread`s are executed in *Parallel* (not the same as *Concurrent*), and they each get slices of time within which to continue execution.

In this way, multiple distinct contexts can be progressed without having to wait for each of them to complete in turn.

## Thread Safety
Those of you familiar with multi-threading will already be aware of the need to enforce careful *thread-safety* when working with multiple `Thread`s.

*ESPressio Threads* makes it easy, providing multiple choices of *Thread-Safe Locks* for you to easily use.

You'll see an example later in this document.

## Basic Usage
ESPressio Threads have been designed with ease of use in mind.

Ultimately, they are a carefully *Managed Encapsulation* of `Task`s, abstracted to operate and interface more alike a true `Thread` in modern desktop and mobile development.

Let's take a look at a really simple implementation:

### Includes...
Before we define our `Thread`, we need to include the required header:
```cpp
    #include <ESPressio_Thread.hpp>
```

### Namespaces...
Given that *ESPressio* Threads uses multi-tier Namespacing throughout, let's declare our Namespace so that we can reference the necessary type identifiers with less code:
```cpp
    using namespace ESPressio::Threads;
```

### A `Thread` type...
With the required header linked, and the namespace defined, we can define a simple `Thread` type, which we shall call `MyFirstThread`:
```cpp
class MyFirstThread : public Thread {
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            // Whatever we want to do within the Loop
        }
};
```
>NOTE: It is not necessary to override `OnInitialization` unless you have a reason. It is virtual, not abstract.

We shall be building from this basic example `class` throughout the rest of this documentation!

So, the above class declaration doesn't really do anything... let's build upon it to illustrate how multiple `Thread`s work:
```cpp
class MyFirstThread : public Thread {
    private:
        int _counter = 0;
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            _counter++; // Increment the counter

            // Let's display some information about our Thread...
            Serial.printf("MyFirstThread::OnLoop() - Thread #%d - On CPU %d, Counter = %d", GetThreadID(), xPortGetCoreID(), _counter);

            delay(1000); // Let's let this Thread wait for 1 second before it loops around again
        }
};
```
With the above changes, any instance of `MyFirstThread` will execute its `OnLoop()` method every one second, and each time it does, it'll increment a *counter*, then print out the following information in the `Serial` console:
- The Thread ID
- Which CPU the Thread is running on
- The value of the *Counter*

Admittedly, this isn't the most practical use of a `Thread`, however, it is an *illustrative* one.

### The `setup()` method...
Let's quickly assemble a program to use `MyFirstThread`:
```cpp
MyFirstThread thread1;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
}
```
That's all there is to it! If you push this program to your (compatible) microcontroller, it will immediately start printing the following into your Serial console (once per second):
```
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 0
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 1
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 2
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 3
```
### What about the `loop()` method?
Your existing `loop()` method will continue to operate exactly as it always has. On the ESP32, the default `loop()` method executes on CPU 1, while you will notice that your instance of `MyFirstThread` (`thread1` in the above sample code) is running on CPU 0.

### The sample code so far...
To make it easier to refer up and down, let's combine all of the code together now:
```cpp
#include <ESPressio_Thread.hpp>

using namespace ESPressio::Threads;

class MyFirstThread : public Thread {
    private:
        int _counter = 0;
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            _counter++; // Increment the counter

            // Let's display some information about our Thread...
            Serial.printf("MyFirstThread::OnLoop() - Thread #%d - On CPU %d, Counter = %d", GetThreadID(), xPortGetCoreID(), _counter);

            delay(1000); // Let's let this Thread wait for 1 second before it loops around again
        }
};

MyFirstThread thread1;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
}
```

### Multiple Threads? No Problem!
So we've created one separate thread (ideally to execute on a separate CPU Core from the default application thread)... but what if we want more threads?

That's really not a problem.

Let's modify the previous example to create multiple Threads:
```cpp
MyFirstThread thread1;
MyFirstThread thread2;
MyFirstThread thread3;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
    thread2.Initialize();
    thread3.Initialize();
}
```

That's all there is to it. The remainder of the README is unchanged from 3.1.6 except for current-version/dependency references and the Serializable PrecisionThread validation statement, which now targets Timing 2.2.8, Units 0.2.7, and Serializable 0.11.3.

### Serializable Precision Thread Example

A consuming application may select a Serializable ESPressio Unit without introducing an ESPressio Serializable dependency into Threads itself:

```cpp
#include <ESPressio_PrecisionThread_Serializable.hpp>

using namespace ESPressio;

using SerializableThreadTime =
    Units::SerializableNanoSeconds<uint64_t>;

class SerializableWorker final :
    public Threads::PrecisionThread<SerializableThreadTime> {

protected:
    void Iterate(
        IterationTime delta,
        IterationTime startTime,
        Threads::SkippedIterationCount skippedIterations
    ) override {
        // delta and startTime are SerializableThreadTime.
    }
};
```

The Threads 3.1.7 release validates this opt-in surface against **Timing 2.2.8**, **Units 0.2.7**, and **Serializable 0.11.3**.
