#pragma once
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ESPressio {


    namespace Threads {

        /// <summary>Lifecycle state of an ESPressio thread.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 4 bytes
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
enum ThreadState {
            Uninitialized,
            Initialized,
            Running,
            Paused,
            Terminating,
            Terminated,
            Destroyed
        };
        /// <summary>Detailed outcome returned when initializing or starting a thread task.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
enum
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 1 bytes [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
class ThreadInitializationStatus : uint8_t {
            Success,
            AlreadyInitialized,
            InvalidState,
            ExitSignalUnavailable,
            TerminationDispatcherUnavailable,
            TerminationDispatchPending,
            TaskCreationFailed,
            ConcurrentInitializationLost,
            TerminatedDuringInitialization,
            InitializationException
        };
        /// <summary>Base exception for ESPressio Threads lifecycle and execution failures.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadException : public std::runtime_error {
            public:
                explicit ThreadException(const char* message)
                    : std::runtime_error(message) {}

                explicit ThreadException(const std::string& message)
                    : std::runtime_error(message) {}
        };
        /// <summary>Base exception for thread registration failures.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadRegistrationException : public ThreadException {
            public:
                explicit ThreadRegistrationException(const char* message)
                    : ThreadException(message) {}

                explicit ThreadRegistrationException(
                    const std::string& message
                ) : ThreadException(message) {}
        };
        /// <summary>Indicates that a requested thread identifier is already registered.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members:
 * - _threadID (uint8_t): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: sizeof(std::runtime_error) + 1 bytes known members [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadDuplicateIDException :
            public ThreadRegistrationException {
            private:
                uint8_t _threadID;

            public:
                explicit ThreadDuplicateIDException(uint8_t threadID)
                    : ThreadRegistrationException(
                        "Thread ID " + std::to_string(threadID) +
                        " is already registered"
                    ),
                    _threadID(threadID) {}
                /// <summary>Returns the conflicting thread identifier.</summary>
                uint8_t GetThreadID() const noexcept {
                    return _threadID;
                }
        };

        /// <summary>Indicates an attempt to register a null thread pointer.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadInvalidRegistrationException :
            public ThreadRegistrationException {
            public:
                ThreadInvalidRegistrationException()
                    : ThreadRegistrationException(
                        "Cannot register a null Thread pointer"
                    ) {}
        };
        /// <summary>Indicates that the maximum number of registered thread identifiers has been exhausted.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadLimitExceededException :
            public ThreadRegistrationException {
            public:
                ThreadLimitExceededException()
                    : ThreadRegistrationException(
                        "ESPressio Threads supports at most 256 registered Threads"
                    ) {}
        };

        /// <summary>Wraps an exception escaping a thread's <c>OnLoop()</c> execution.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(std::runtime_error) [0 bytes dynamic allocation]
 * Members:
 * - _cause (std::exception_ptr): sizeof(std::exception_ptr) [0 bytes dynamic allocation]
 * Total Memory: sizeof(std::runtime_error) + sizeof(std::exception_ptr) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ThreadExecutionException : public ThreadException {
            private:
                std::exception_ptr _cause;
            public:
                explicit ThreadExecutionException(
                    std::exception_ptr cause
                ) : ThreadException("Thread OnLoop execution failed"),
                    _cause(std::move(cause)) {}

                /// <summary>Returns the captured underlying exception.</summary>
                const std::exception_ptr& GetCause() const noexcept {
                    return _cause;
                }
                /// <summary>Rethrows the captured underlying exception when present.</summary>
                void RethrowCause() const {
                    if (_cause != nullptr) {
                        std::rethrow_exception(_cause);
                    }
                }
        };
        /// <summary>Common lifecycle, configuration, and callback contract implemented by all ESPressio thread types.</summary>
/**
 * ESPressio Memory Audit
 * Members: none; polymorphic interface/object includes vptr storage where not supplied by a base.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
class IThread  {
            private:
                std::atomic<bool> _automaticCleanupClaimed{false};

            public:
            // Type Defs
                /// <summary>Callback receiving the thread responsible for a lifecycle notification.</summary>
                typedef std::function<void(IThread*)> ThreadCallback;
                /// <summary>Callback receiving a thread and its previous and new lifecycle states.</summary>
                typedef std::function<void(IThread*, ThreadState, ThreadState)> ThreadStateChangeCallback;
                /// <summary>Callback receiving a thread whose initialization failed and its detailed status.</summary>
                typedef std::function<void(
                    IThread*,
                    ThreadInitializationStatus
                )> ThreadInitializationFailedCallback;
                /// <summary>Callback receiving a thread whose loop failed and the captured exception.</summary>
                typedef std::function<void(
                    IThread*,
                    std::exception_ptr
                )> ThreadExecutionFailedCallback;
            // Destructor

                IThread() = default;
                IThread(const IThread&) = delete;
                IThread& operator=(const IThread&) = delete;
                IThread(IThread&&) = delete;
                IThread& operator=(IThread&&) = delete;
                virtual ~IThread() {}

            // Methods
                /// <summary>Initializes the thread's underlying execution resources without necessarily starting its loop.</summary>
                virtual ThreadInitializationStatus Initialize() = 0;
                /// <summary>Requests termination of the thread and its underlying task lifecycle.</summary>
                virtual void Terminate() = 0;
                /// <summary>Starts an initialized thread or resumes a paused thread, initializing it first when necessary.</summary>
                virtual ThreadInitializationStatus Start() = 0;

                /// <summary>Pauses loop execution when the thread is running.</summary>
                virtual void Pause() = 0;
                /// <summary>Atomically claims an eligible thread object for manager-driven automatic cleanup.</summary>
                virtual bool TryClaimAutomaticCleanup() {
                    if (!GetFreeOnTerminate()) {
                        return false;
                    }
                    bool expected = false;
                    return _automaticCleanupClaimed.compare_exchange_strong(
                        expected,
                        true,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                }

            // Getters

                /// <summary>Returns the execution core identifier assigned to the thread.</summary>
                virtual int GetCoreID() = 0;
                /// <summary>Returns the configured task stack size.</summary>
                virtual uint32_t GetStackSize() = 0;

                /// <summary>Returns the configured thread priority.</summary>
                virtual unsigned int GetPriority() = 0;

                /// <summary>Returns the thread's unique manager identifier.</summary>
                virtual uint8_t GetThreadID() = 0;
                /// <summary>Returns the current thread lifecycle state.</summary>
                virtual ThreadState GetThreadState() = 0;

                /// <summary>Indicates whether the manager should reclaim the thread object after termination.</summary>
                virtual bool GetFreeOnTerminate() = 0;

                /// <summary>Indicates whether initialization should automatically start thread execution.</summary>
                virtual bool GetStartOnInitialize() = 0;
            // Utility Getters

                /// <summary>Indicates whether the thread is currently running.</summary>
                bool IsRunning() { return GetThreadState() == ThreadState::Running; }

                /// <summary>Indicates whether the thread is currently paused.</summary>
                bool IsPaused() { return GetThreadState() == ThreadState::Paused; }

                /// <summary>Indicates whether termination has been requested and is in progress.</summary>
                bool IsTerminating() { return GetThreadState() == ThreadState::Terminating; }

                /// <summary>Indicates whether thread execution has completed termination.</summary>
                bool IsTerminated() { return GetThreadState() == ThreadState::Terminated; }

            // Callback Getters
                /// <summary>Returns the callback invoked when the thread object is destroyed.</summary>
                virtual ThreadCallback GetOnDestroy() = 0;

                /// <summary>Returns the callback invoked after successful initialization.</summary>
                virtual ThreadCallback GetOnInitialize() = 0;

                /// <summary>Returns the callback invoked when the thread starts or resumes.</summary>
                virtual ThreadCallback GetOnStart() = 0;
                /// <summary>Returns the callback invoked when the thread is paused.</summary>
                virtual ThreadCallback GetOnPause() = 0;

                /// <summary>Returns the callback invoked when the loop enters terminating/terminated lifecycle processing.</summary>
                virtual ThreadCallback GetOnTerminate() = 0;
                /// <summary>Returns the callback invoked after the underlying task has fully completed termination.</summary>
                virtual ThreadCallback GetOnTerminated() { return nullptr; }
                /// <summary>Returns the callback invoked when initialization does not succeed.</summary>
                virtual ThreadInitializationFailedCallback
                GetOnInitializationFailed() { return nullptr; }

                /// <summary>Returns the callback invoked when thread loop execution throws.</summary>
                virtual ThreadExecutionFailedCallback
                GetOnExecutionFailed() { return nullptr; }
                /// <summary>Returns the callback invoked on lifecycle state transitions.</summary>
                virtual ThreadStateChangeCallback GetOnStateChange() = 0;

            // Setters

                /// <summary>Sets the execution core affinity requested for the thread.</summary>
                virtual void SetCoreID(int value) = 0;

                /// <summary>Sets the task stack size used for thread execution.</summary>
                virtual void SetStackSize(uint32_t value) = 0;
                /// <summary>Sets the task priority used for thread execution.</summary>
                virtual void SetPriority(unsigned int value) = 0;

                /// <summary>Sets whether the manager may reclaim the thread object after termination.</summary>
                virtual void SetFreeOnTerminate(bool value) = 0;

                /// <summary>Sets whether thread execution starts automatically after initialization.</summary>
                virtual void SetStartOnInitialize(bool value) = 0;
            // Callback Setters

                /// <summary>Sets the callback invoked when the thread object is destroyed.</summary>
                virtual void SetOnDestroy(ThreadCallback) = 0;
                /// <summary>Sets the callback invoked after successful initialization.</summary>
                virtual void SetOnInitialize(ThreadCallback) = 0;

                /// <summary>Sets the callback invoked when the thread starts or resumes.</summary>
                virtual void SetOnStart(ThreadCallback) = 0;
                /// <summary>Sets the callback invoked when the thread is paused.</summary>
                virtual void SetOnPause(ThreadCallback) = 0;

                /// <summary>Sets the callback invoked as loop termination begins.</summary>
                virtual void SetOnTerminate(ThreadCallback) = 0;
                /// <summary>Sets the callback invoked after the underlying task has completed termination.</summary>
                virtual void SetOnTerminated(ThreadCallback) {}
                /// <summary>Sets the callback invoked when initialization does not succeed.</summary>
                virtual void SetOnInitializationFailed(
                    ThreadInitializationFailedCallback
                ) {}

                /// <summary>Sets the callback invoked when thread loop execution throws.</summary>
                virtual void SetOnExecutionFailed(
                    ThreadExecutionFailedCallback
                ) {}
                /// <summary>Sets the callback invoked whenever the thread lifecycle state changes.</summary>
                virtual void SetOnStateChange(ThreadStateChangeCallback) = 0;
        };

    }
}
