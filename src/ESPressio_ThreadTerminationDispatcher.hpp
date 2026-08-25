#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <ESPressio_IObservable.hpp>

#include "ESPressio_IThreadTerminationDispatcherObserver.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE 2000
#endif

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY 2
#endif

#ifndef ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH
    #define ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH 32
#endif

namespace ESPressio {
namespace Threads {

    class Thread;


    class ThreadTerminationDispatcher {

    private:
        class DispatcherObservable final :
            public Observable::ThreadSafeObservable {

        private:
            template <typename TCallback>
            void NotifyObservers(
                TCallback callback
            ) {
                ExecuteNotification(
                    [&](NotificationContext& notification) {
                        notification.WithObservers<
                            IThreadTerminationDispatcherObserver
                        >(
                            [&](IThreadTerminationDispatcherObserver* observer) {
                                try {
                                    callback(observer);
                                } catch (...) {
                                }
                            }
                        );
                    }
                );
            }

        public:
            void Initialized(bool available) {
                NotifyObservers(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        observer->
                            OnThreadTerminationDispatcherInitialized(
                                available
                            );
                    }
                );
            }

            void Queued(
                const ThreadManagerThreadSnapshot& snapshot
            ) {
                NotifyObservers(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        observer->
                            OnThreadTerminationDispatchQueued(
                                snapshot
                            );
                    }
                );
            }

            void QueueFailed(
                const ThreadManagerThreadSnapshot& snapshot
            ) {
                NotifyObservers(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        observer->
                            OnThreadTerminationDispatchQueueFailed(
                                snapshot
                            );
                    }
                );
            }

            void Started(
                const ThreadManagerThreadSnapshot& snapshot
            ) {
                NotifyObservers(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        observer->
                            OnThreadTerminationDispatchStarted(
                                snapshot
                            );
                    }
                );
            }

            void Completed(
                const ThreadManagerThreadSnapshot& snapshot
            ) {
                NotifyObservers(
                    [&](IThreadTerminationDispatcherObserver* observer) {
                        observer->
                            OnThreadTerminationDispatchCompleted(
                                snapshot
                            );
                    }
                );
            }
        };


        struct DispatchRecord {
            Thread* ThreadPointer = nullptr;
            ThreadManagerThreadSnapshot Snapshot;
        };


        QueueHandle_t _queue = nullptr;
        TaskHandle_t _taskHandle = nullptr;
        mutable std::mutex _initializationMutex;

        std::shared_ptr<DispatcherObservable>
            _observable =
                std::make_shared<DispatcherObservable>();


        ThreadTerminationDispatcher() = default;

        bool _initialize();

        static void _taskEntry(
            void* parameter
        );

        void _loop();


    public:
        ThreadTerminationDispatcher(
            const ThreadTerminationDispatcher&
        ) = delete;

        ThreadTerminationDispatcher&
        operator=(
            const ThreadTerminationDispatcher&
        ) = delete;


        static ThreadTerminationDispatcher*
        GetInstance();


        bool IsAvailable() const;
        bool EnsureAvailable();
        bool IsCurrentTask() const;
        uint32_t GetMinimumFreeStackBytes() const;
        bool Dispatch(Thread* thread);


        Observable::ObserverHandlePtr
        RegisterObserver(
            IThreadTerminationDispatcherObserver* observer
        ) {
            auto handle =
                _observable->RegisterObserver(
                    observer
                );

            if (observer != nullptr && IsAvailable()) {
                try {
                    observer->
                        OnThreadTerminationDispatcherInitialized(
                            true
                        );
                } catch (...) {
                }
            }

            return handle;
        }


        void UnregisterObserver(
            IThreadTerminationDispatcherObserver* observer
        ) {
            _observable->UnregisterObserver(
                observer
            );
        }
    };

}
}
