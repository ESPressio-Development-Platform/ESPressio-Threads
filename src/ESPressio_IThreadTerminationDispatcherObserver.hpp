#pragma once

#include <ESPressio_IObserver.hpp>

#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Observer contract for asynchronous thread-termination dispatcher lifecycle notifications.</summary>
    class IThreadTerminationDispatcherObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadTerminationDispatcherObserver() = default;

        /// <summary>Called when the termination dispatcher initialization attempt completes.</summary>
        virtual void OnThreadTerminationDispatcherInitialized(
            bool
        ) {}

        /// <summary>Called after a thread termination request is queued for dispatcher processing.</summary>
        virtual void OnThreadTerminationDispatchQueued(
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called when a termination request cannot be queued.</summary>
        virtual void OnThreadTerminationDispatchQueueFailed(
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called when dispatcher processing begins for a queued termination.</summary>
        virtual void OnThreadTerminationDispatchStarted(
            const ThreadManagerThreadSnapshot&
        ) {}

        /// <summary>Called after dispatcher processing completes for a queued termination.</summary>
        virtual void OnThreadTerminationDispatchCompleted(
            const ThreadManagerThreadSnapshot&
        ) {}
    };

}
}
