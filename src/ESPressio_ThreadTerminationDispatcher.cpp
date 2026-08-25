#include "ESPressio_ThreadTerminationDispatcher.hpp"
#include "ESPressio_Thread.hpp"
#include <ESPressio_Task.hpp>

namespace ESPressio {
namespace Threads {

    namespace {

        ThreadManagerThreadSnapshot
        SnapshotThread(
            Thread* thread
        ) {
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

        if (_queue != nullptr && _taskHandle != nullptr) {
            return true;
        }

        QueueHandle_t queue = xQueueCreate(
            ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH,
            sizeof(DispatchRecord)
        );

        if (queue == nullptr) {
            _observable->Initialized(false);
            return false;
        }

        /*
         * Publish dependencies before the worker can become runnable. The
         * ESPressio Task runtime may make the FreeRTOS task runnable on the
         * other core before Create() returns.
         */
        _queue = queue;

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
            _queue = nullptr;
            vQueueDelete(queue);
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

        Task::TaskRuntime::Delete(nullptr);
    }


    void ThreadTerminationDispatcher::_loop() {
        for (;;) {
            DispatchRecord record;

            if (
                xQueueReceive(
                    _queue,
                    &record,
                    portMAX_DELAY
                ) != pdTRUE ||
                record.ThreadPointer == nullptr
            ) {
                continue;
            }

            _observable->Started(record.Snapshot);

            record.ThreadPointer->_dispatchTermination();

            /*
             * Do not dereference the Thread after termination dispatch:
             * automatic GC can now own its eventual destruction.
             */
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
        return _queue != nullptr && _taskHandle != nullptr;
    }


    bool ThreadTerminationDispatcher::EnsureAvailable() {
        return _initialize();
    }


    bool ThreadTerminationDispatcher::IsCurrentTask() const {
        std::lock_guard<std::mutex> lock(_initializationMutex);
        return
            _taskHandle != nullptr &&
            Task::TaskRuntime::Current() == _taskHandle;
    }


    bool ThreadTerminationDispatcher::Dispatch(Thread* thread) {
        if (thread == nullptr) {
            return false;
        }

        if (!EnsureAvailable()) {
            return false;
        }

        QueueHandle_t queue = nullptr;
        {
            std::lock_guard<std::mutex> lock(_initializationMutex);
            queue = _queue;
        }

        if (queue == nullptr) {
            return false;
        }

        DispatchRecord record;
        record.ThreadPointer = thread;
        record.Snapshot = SnapshotThread(thread);

        const bool queued =
            xQueueSend(queue, &record, portMAX_DELAY) == pdTRUE;

        if (queued) {
            _observable->Queued(record.Snapshot);
        } else {
            _observable->QueueFailed(record.Snapshot);
        }

        return queued;
    }

}
}
