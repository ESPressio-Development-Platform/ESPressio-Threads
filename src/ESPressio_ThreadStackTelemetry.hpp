#pragma once

#include <cstdint>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ESPressio_IThread.hpp"

namespace ESPressio::Threads {

struct ThreadStackTelemetry {
    uint32_t ConfiguredBytes{0};
    uint32_t MinimumFreeBytes{0};
    bool Available{false};
};

inline ThreadStackTelemetry GetThreadStackTelemetry(IThread& thread) noexcept {
    ThreadStackTelemetry telemetry;
    telemetry.ConfiguredBytes = thread.GetStackSize();

    char taskName[configMAX_TASK_NAME_LEN > 0 ? configMAX_TASK_NAME_LEN : 16];
    std::snprintf(
        taskName,
        sizeof(taskName),
        "thread%u",
        static_cast<unsigned int>(thread.GetThreadID())
    );

    TaskHandle_t handle = xTaskGetHandle(taskName);
    if (handle == nullptr) {
        return telemetry;
    }

    telemetry.MinimumFreeBytes =
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(handle));
    telemetry.Available = true;
    return telemetry;
}

} // namespace ESPressio::Threads
