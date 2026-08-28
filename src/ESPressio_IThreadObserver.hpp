#pragma once

#include <exception>

#include <ESPressio_IObserver.hpp>

#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Observer contract for individual thread lifecycle, initialization, task-exit, and execution-failure notifications.</summary>
    class IThreadObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadObserver() = default;

        /// <summary>Called whenever a thread transitions between lifecycle states.</summary>
        virtual void OnThreadStateChanged(
            IThread*,
            ThreadState,
            ThreadState
        ) {}

        /// <summary>Called when a thread enters the uninitialized state.</summary>
        virtual void OnThreadUninitialized(IThread*) {}
        /// <summary>Called after successful thread initialization.</summary>
        virtual void OnThreadInitialized(IThread*) {}
        /// <summary>Called when thread execution starts or resumes.</summary>
        virtual void OnThreadStarted(IThread*) {}
        /// <summary>Called when thread execution is paused.</summary>
        virtual void OnThreadPaused(IThread*) {}
        /// <summary>Called when thread termination is requested.</summary>
        virtual void OnThreadTerminationRequested(IThread*) {}
        /// <summary>Called when the thread reaches terminated state.</summary>
        virtual void OnThreadTerminated(IThread*) {}
        /// <summary>Called when the thread object is destroyed.</summary>
        virtual void OnThreadDestroyed(IThread*) {}
        /// <summary>Called after the underlying task function exits.</summary>
        virtual void OnThreadTaskExited(IThread*) {}

        /// <summary>Called when thread initialization returns a non-success status.</summary>
        virtual void OnThreadInitializationFailed(
            IThread*,
            ThreadInitializationStatus
        ) {}

        /// <summary>Called when thread loop execution fails with an exception.</summary>
        virtual void OnThreadExecutionFailed(
            IThread*,
            std::exception_ptr
        ) {}
    };

}
}
