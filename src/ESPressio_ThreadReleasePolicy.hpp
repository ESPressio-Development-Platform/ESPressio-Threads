#pragma once

#include <cstdint>

namespace ESPressio {
    namespace Threads {

        enum class ThreadReleasePolicy : uint8_t {
            ExplicitRelease,
            ReleaseOnTerminate
        };

    }
}
