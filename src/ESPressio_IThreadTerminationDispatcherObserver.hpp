#pragma once

#include <ESPressio_IObserver.hpp>

#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Observer contract for asynchronous thread-termination dispatcher lifecycle notifications.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Observable::IObserver) [0 bytes dynamic allocation]
 * Members: none; polymorphic interface/object includes vptr storage where not supplied by a base.
 * Total Memory: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
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
