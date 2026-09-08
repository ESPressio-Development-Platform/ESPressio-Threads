#pragma once

#include <cstdint>

namespace ESPressio {
    namespace Threads {

        /// <summary>Controls whether thread object lifetime is explicitly owned by the caller or automatically released after termination.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum
class ThreadReleasePolicy : uint8_t {
            ExplicitRelease,
            ReleaseOnTerminate
        };

    }
}
