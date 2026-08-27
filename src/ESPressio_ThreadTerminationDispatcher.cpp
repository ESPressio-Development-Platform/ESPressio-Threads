#include "ESPressio_ThreadTerminationDispatcher.hpp"
#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"
#include <ESPressio_Task.hpp>

namespace ESPressio {
namespace Threads {

    namespace {

        ThreadManagerThreadSnapshot SnapshotThread(Thread* thread) {
            ThreadManagerThreadSnapshot snapshot;

            if (thread == nullptr) {
                return snapshot;
            }

            snapshot.ThreadID = thread->GetThreadID();
            snapshot.CoreID = thread->GetCoreID();
            snapshot.State = thread->GetThreadState();
            snapshot.FreeOnTerminate = thread->GetFreeOnTerminate();
            snapshot.StartOnInitialize = thread->GetStartOnInitialize();

            return snapshot;
        }

    }

    bool ThreadTerminationDispatcher::_initialize() {
        std::lock_guard<std::mutex> lock(_initializationMutex);

        if (_queue != nullptr &&
            _taskHandle != System::Execution::InvalidExecutionHandle) {
            return true;
        }

        auto queue = System::Queue::Create<DispatchRecord>(
            ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH
        );

        if (queue == nullptr) {
            _observable->Initialized(false);
            return false;
        }

        /*
         * Publish dependencies before the worker can become runnable. The
         * installed execution provider may make the worker runnable on another
         * processor before Create() returns.
         */
        _queue = std::move(queue);

        Task::TaskConfiguration configuration;
        configuration.Name = "threadTerminationDispatcher";
        configuration.StackSize = ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE;
        configuration.Priority = ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY;

        const auto creation = Task::TaskRuntime::Create(
            _taskEntry,
            this,
            configuration
        );

        if (!creation) {
            _queue.reset();
            _observable->Initialized(false);
            return false;
        }

        _taskHandle = creation.Handle;
        _observable->Initialized(true);
        return true;
    }

    void ThreadTerminationDispatcher::_taskEntry(void* parameter) {
        ThreadTerminationDispatcher* dispatcher =
            static_cast<ThreadTerminationDispatcher*>(parameter);

        if (dispatcher != nullptr) {
            dispatcher->_loop();
        }

        Task::TaskRuntime::Delete(System::Execution::InvalidExecutionHandle);
    }

    void ThreadTerminationDispatcher::_loop() {
        for (;;) {
            DispatchRecord record;

            if (
                _queue == nullptr ||
                !_queue->Receive(&record) ||
                record.ThreadPointer == nullptr
            ) {
                continue;
            }

            _observable->Started(record.Snapshot);

            record.ThreadPointer->_dispatchTermination();

            try {
                ThreadManager::GetInstance()->CleanUpWithResult();
            } catch (...) {
                /*
                 * Reclamation failure must never terminate this permanent
                 * infrastructure execution context. Eligible objects remain
                 * manager-owned and can be reclaimed by a later cleanup pass.
                 */
            }

            _observable->Completed(record.Snapshot);
        }
    }

    ThreadTerminationDispatcher*
    ThreadTerminationDispatcher::GetInstance() {
        static ThreadTerminationDispatcher instance;
        return &instance;
    }

    bool ThreadTerminationDispatcher::IsAvailable() const {
        std::lock_guard<std::mutex> lock(_initializationMutex);
        return _queue != nullptr &&
            _taskHandle != System::Execution::InvalidExecutionHandle;
    }

    bool ThreadTerminationDispatcher::EnsureAvailable() {
        return _initialize();
    }

    bool ThreadTerminationDispatcher::IsCurrentTask() const {
        std::lock_guard<std::mutex> lock(_initializationMutex);
        return
            _taskHandle != System::Execution::InvalidExecutionHandle &&
            Task::TaskRuntime::Current() == _taskHandle;
    }

    uint32_t ThreadTerminationDispatcher::GetMinimumFreeStackBytes() const {
        std::lock_guard<std::mutex> lock(_initializationMutex);
        if (_taskHandle == System::Execution::InvalidExecutionHandle) {
            return 0;
        }
        return Task::TaskRuntime::MinimumFreeStack(_taskHandle);
    }

    bool ThreadTerminationDispatcher::Dispatch(Thread* thread) {
        if (thread == nullptr) {
            return false;
        }

        if (!EnsureAvailable()) {
            return false;
        }

        DispatchRecord record;
        record.ThreadPointer = thread;
        record.Snapshot = SnapshotThread(thread);

        System::PlatformResult queued = System::PlatformResult::Failed(
            System::PlatformStatus::Unavailable
        );
        {
            std::lock_guard<std::mutex> lock(_initializationMutex);
            if (_queue != nullptr) {
                queued = _queue->Send(
                    &record,
                    System::Synchronization::WaitForever
                );
            }
        }

        if (queued) {
            _observable->Queued(record.Snapshot);
        } else {
            _observable->QueueFailed(record.Snapshot);
        }

        return static_cast<bool>(queued);
    }

}
}
