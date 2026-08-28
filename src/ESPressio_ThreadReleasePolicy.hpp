#pragma once

#include <cstdint>

namespace ESPressio {
    namespace Threads {

        /// <summary>Controls whether thread object lifetime is explicitly owned by the caller or automatically released after termination.</summary>
        enum class ThreadReleasePolicy : uint8_t {
            ExplicitRelease,
            ReleaseOnTerminate
        };

    }
}
