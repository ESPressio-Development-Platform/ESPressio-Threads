#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <HostRuntime.hpp>
#include <ESPressio_SystemPlatformClock.hpp>
class ManualClock final : public ESPressio::System::Clock::IMonotonicClock {
public:
    std::atomic<std::uint64_t> Value{0};
    std::uint64_t NowNanoseconds() const noexcept override { return Value.load(); }
    std::uint64_t ResolutionNanoseconds() const noexcept override { return 1; }
    bool IsInterruptSafe() const noexcept override { return true; }
};
template<class Predicate> void Await(Predicate predicate) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while (!predicate()) {
        if (std::chrono::steady_clock::now()>end) { std::fputs("Timed out waiting for contract evidence\n",stderr); std::abort(); }
        std::this_thread::yield();
    }
}
