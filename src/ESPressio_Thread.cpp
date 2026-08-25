#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"
#include "ESPressio_ThreadTerminationDispatcher.hpp"

namespace ESPressio {

    namespace Threads {
        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            try {
                _lifecycleObservable =
                    std::make_shared<LifecycleObservable>();
                SetCoreID(
                    ThreadManager::GetInstance()->AddThread(this, &_threadID)
                );
            } catch (...) {
                const std::exception_ptr constructionFailure =
                    std::current_exception();
                try {
                    ThreadManager::GetInstance()->RemoveThread(this);
                } catch (...) {
                    // Preserve the original construction failure.
                }

                if (_taskExited != nullptr) {
                    vSemaphoreDelete(_taskExited);
                    _taskExited = nullptr;
                }

                std::rethrow_exception(constructionFailure);
            }
        }
        Thread::~Thread() {
            SetFreeOnTerminate(false);
            _waitForTerminationDispatch();
            SetThreadState(ThreadState::Destroyed);
            TOnThreadEvent onDestroy = GetOnDestroy();
            if (onDestroy != nullptr) {
                try {
                    onDestroy(this);
                } catch (...) {
                    // Destructors must not allow user callbacks to terminate
                    // the program or interrupt resource cleanup.
                }
            }
            _deleteTask();
            if (_taskExited != nullptr) {
                vSemaphoreDelete(_taskExited);
                _taskExited = nullptr;
            }
            ThreadManager::GetInstance()->RemoveThread(this);
        }

        void Thread::_requestGarbageCollection() {
            /*
             * Compatibility shim for the historical Thread::GarbageCollect()
             * API. Automatic reclamation no longer owns a dedicated worker;
             * ThreadManager contains the actual claim/remove/delete semantics
             * and already defers cleanup while a manager iteration is active.
             */
            ThreadManager::GetInstance()->CleanUp();
        }

        bool Thread::_isTerminationDispatcherAvailable() {
            return ThreadTerminationDispatcher::GetInstance()->EnsureAvailable();
        }

        bool Thread::_isCurrentTerminationDispatcherTask() {
            return ThreadTerminationDispatcher::GetInstance()->IsCurrentTask();
        }
        bool Thread::_queueTerminationDispatch(Thread* thread) {
            return ThreadTerminationDispatcher::GetInstance()->Dispatch(thread);
        }

        void Thread::_dispatchTermination() {
            /*
             * Normal task exit leaves the ESPressio-owned task suspended.
             * The dispatcher is the sole context that deletes that task,
             * ensuring no Thread object can be reclaimed while its own
             * FreeRTOS stack is still executing.
             */
            _deleteTask();

            const bool terminated =
                GetThreadState() == ThreadState::Terminated;
            const SemaphoreHandle_t taskExited = _taskExited;
            TOnThreadEvent onTerminated = GetOnTerminated();
            if (terminated && onTerminated != nullptr) {
                try {
                    onTerminated(this);
                } catch (...) {
                    // User callbacks must not terminate the dispatcher task.
                }
            }
            if (terminated) {
                try {
                    _lifecycleObservable->NotifyTaskExited(this);
                } catch (...) {
                    // Observer failures must not terminate the dispatcher.
                }
            }

            if (taskExited != nullptr) {
                xSemaphoreGive(taskExited);
            }

            /*
             * The dispatcher performs manager-owned automatic reclamation only
             * after this method returns. Clearing the pending flag here lets
             * explicitly-owned Thread destructors complete once their native
             * task has been deleted, while ReleaseOnTerminate objects remain
             * manager-owned until the dispatcher's cleanup pass claims them.
             */
            _terminationDispatchPending.store(false, std::memory_order_release);
        }

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) {
                _requestGarbageCollection();
            }
        }
    }

}
