#pragma once

#include <cstddef>
#include <cstdint>

#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

    /// <summary>Stable snapshot of thread identity, placement, lifecycle, and ownership configuration.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - ThreadID (uint8_t): 1 bytes [0 bytes dynamic allocation]
 * - CoreID (int): 4 bytes [0 bytes dynamic allocation]
 * - State (ThreadState): 4 bytes [0 bytes dynamic allocation]
 * - FreeOnTerminate (bool): 1 bytes [0 bytes dynamic allocation]
 * - StartOnInitialize (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct ThreadManagerThreadSnapshot {
        uint8_t ThreadID = 0;
        int CoreID = 0;
        ThreadState State = ThreadState::Uninitialized;
        bool FreeOnTerminate = false;
        bool StartOnInitialize = true;
    };


    /// <summary>Summary of one manager cleanup pass, including claims, removals, deletions, deferral, and registry counts.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - ThreadsExamined (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadsClaimed (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadsRemoved (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadsDeleted (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - WasDeferred (bool): 1 bytes [0 bytes dynamic allocation]
 * - ActiveIterationCount (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadCountBefore (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadCountAfter (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct ThreadManagerCleanupResult {
        std::size_t ThreadsExamined = 0;
        std::size_t ThreadsClaimed = 0;
        std::size_t ThreadsRemoved = 0;
        std::size_t ThreadsDeleted = 0;

        bool WasDeferred = false;
        std::size_t ActiveIterationCount = 0;

        std::size_t ThreadCountBefore = 0;
        std::size_t ThreadCountAfter = 0;
    };


    /// <summary>Summary of one manager-wide initialization pass.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - ThreadsExamined (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadsInitializedSuccessfully (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - ThreadsInitializationFailed (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct ThreadManagerInitializationResult {
        std::size_t ThreadsExamined = 0;
        std::size_t ThreadsInitializedSuccessfully = 0;
        std::size_t ThreadsInitializationFailed = 0;
    };

}
}
