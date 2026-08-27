#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"
#include "ESPressio_ThreadTerminationDispatcher.hpp"

#include <ESPressio_Memory.hpp>
#include <ESPressio_Synchronization.hpp>

namespace ESPressio {

    namespace Threads {
        Thread::Thread() : _threadID(0) {
            try {
                _taskExited = System::Synchronization::CreateBinarySignal();
                _taskStartGate = System::Synchronization::CreateBinarySignal();
                _lifecycleObservable =
                    System::Memory::MakeShared<
                        LifecycleObservable,
                        System::Memory::MemoryPolicy::ExternalPreferred
                    >();
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
                _taskStartGate.reset();
                _taskExited.reset();
                std::rethrow_exception(constructionFailure);
            }
        }

        Thread::~Thread() {
            SetFreeOnTerminate(false);
            _waitForTerminationDispatch();
            SetThreadState(ThreadState::Destroyed);
            StableCallback<TOnThreadEvent> onDestroy;
            {
                std::lock_guard<std::mutex> lock(_callbackMutex);
                onDestroy = _onDestroy;
            }
            if (onDestroy != nullptr) {
                try {
                    (*onDestroy)(this);
                } catch (...) {
                    // Destructors must not allow user callbacks to terminate
                    // the program or interrupt resource cleanup.
                }
            }
            _deleteTask();
            _taskStartGate.reset();
            _taskExited.reset();
            ThreadManager::GetInstance()->RemoveThread(this);
        }

        void Thread::_requestGarbageCollection() {
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
            _deleteTask();

            const bool terminated =
                GetThreadState() == ThreadState::Terminated;
            StableCallback<TOnThreadEvent> onTerminated;
            {
                std::lock_guard<std::mutex> lock(_callbackMutex);
                onTerminated = _onTerminated;
            }
            if (terminated && onTerminated != nullptr) {
                try {
                    (*onTerminated)(this);
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

            if (_taskExited != nullptr) {
                (void)_taskExited->Give();
            }

            _terminationDispatchPending.store(false, std::memory_order_release);
        }

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) {
                _requestGarbageCollection();
            }
        }
    }

}
