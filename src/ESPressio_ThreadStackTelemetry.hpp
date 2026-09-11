#pragma once
#include "ESPressio_IThread.hpp"
namespace ESPressio::Threads {
/// <summary>Portable physical-task stack evidence. No native handle lookup, task name registry or TLS access.</summary>
struct ThreadStackTelemetry final {
    std::uint32_t MinimumFreeBytes=0;
    bool Available=false;
};
inline ThreadStackTelemetry GetThreadStackTelemetry(const IThread& thread) {
    const auto diagnostics=thread.GetDiagnostics();
    return {diagnostics.MinimumFreeStackBytes,diagnostics.StackTelemetryAvailable};
}
}
