#pragma once

#include <exception>

#include <ESPressio_IObserver.hpp>

#include "ESPressio_IThread.hpp"
#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Observer contract for thread registration, cleanup, and manager initialization lifecycle notifications.</summary>
    class IThreadManagerObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadManagerObserver() = default;

        /// <summary>Called after a thread is successfully registered.</summary>
        virtual void OnThreadRegistered(
            IThread*,
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called when thread registration fails.</summary>
        virtual void OnThreadRegistrationFailed(
            IThread*,
            std::exception_ptr
        ) {}

        /// <summary>Called after a thread is removed from the manager registry.</summary>
        virtual void OnThreadRemoved(
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called when manager-driven automatic cleanup successfully claims a thread object.</summary>
        virtual void OnThreadCleanupClaimed(
            IThread*,
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called when cleanup is deferred because the thread cannot yet be safely reclaimed.</summary>
        virtual void OnThreadCleanupDeferred(
            const ThreadManagerCleanupResult&
        ) {}

        /// <summary>Called immediately before manager-driven cleanup begins.</summary>
        virtual void OnThreadCleanupStarted(
            const ThreadManagerCleanupResult&
        ) {}

        /// <summary>Called after manager-driven cleanup completes successfully.</summary>
        virtual void OnThreadCleanupCompleted(
            const ThreadManagerCleanupResult&
        ) {}

        /// <summary>Called when manager-driven cleanup fails.</summary>
        virtual void OnThreadCleanupFailed(
            const ThreadManagerCleanupResult&,
            std::exception_ptr
        ) {}

        /// <summary>Called when a manager-wide initialization pass completes.</summary>
        virtual void OnThreadManagerInitializationCompleted(
            const ThreadManagerInitializationResult&
        ) {}
    };

}
}
