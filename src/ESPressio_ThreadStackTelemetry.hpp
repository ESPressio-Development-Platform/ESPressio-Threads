#pragma once

#include <cstdint>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ESPressio_IThread.hpp"

namespace ESPressio::Threads {

/// <summary>Snapshot of configured thread stack size and FreeRTOS minimum-free-stack telemetry.</summary>
struct ThreadStackTelemetry {
    uint32_t ConfiguredBytes{0};
    uint32_t MinimumFreeBytes{0};
    bool Available{false};
};

/// <summary>Queries stack high-water telemetry for the FreeRTOS task associated with an ESPressio thread.</summary>
/// <returns>A telemetry snapshot whose <c>Available</c> flag indicates whether the underlying task could be resolved.</returns>
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
