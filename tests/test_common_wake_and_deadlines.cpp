#include "TestRuntime.hpp"
#include <ESPressio_ThreadWith.hpp>
#include <cassert>
using namespace ESPressio;
using namespace ESPressio::Threads;
class CountSignals final : public System::Synchronization::ISynchronizationProvider {
    HostRuntime& Runtime;
    struct Signal final : System::Synchronization::ISignal {
        std::unique_ptr<System::Synchronization::ISignal> Inner;
        CountSignals& Owner;
        Signal(std::unique_ptr<System::Synchronization::ISignal> inner,CountSignals& owner):Inner(std::move(inner)),Owner(owner) {}
        System::PlatformResult Give() noexcept override { ++Owner.Ordinary; return Inner->Give(); }
        System::PlatformResult GiveFromInterrupt() noexcept override { ++Owner.Interrupt; return Inner->GiveFromInterrupt(); }
        System::PlatformResult Wait(std::uint32_t timeout=UINT32_MAX) noexcept override {
            Owner.LastTimeout=timeout; return Inner->Wait(timeout);
        }
        System::PlatformResult Reset() noexcept override { ++Owner.Resets; return Inner->Reset(); }
    };
public:
    std::atomic<unsigned> Ordinary{0},Interrupt{0},Resets{0};
    std::atomic<std::uint32_t> LastTimeout{0};
    explicit CountSignals(HostRuntime& runtime):Runtime(runtime) { System::Synchronization::SetProvider(this); }
    ~CountSignals() { System::Synchronization::SetProvider(&Runtime); }
    std::unique_ptr<System::Synchronization::ISignal> CreateBinarySignal(bool set=false) override {
        return std::make_unique<Signal>(Runtime.CreateBinarySignal(set),*this);
    }
};
struct NoticeTag {};
struct Notice final : ThreadCapability {
    using CapabilityTag=NoticeTag;
    std::atomic<bool> Pending{false};
    std::atomic<unsigned> Serviced{0},Discarded{0};
    ThreadHostServices Host;
    ThreadStatus Initialize(const ThreadHostServices& host) { Host=host; return ThreadStatus::Success; }
    bool TryPublish(bool interrupt=false) {
        ThreadAdmissionClaim claim(Host);
        if (!claim) return false;
        bool expected=false;
        if (!Pending.compare_exchange_strong(expected,true)) return false;
        claim.Release();
        if (interrupt) Host.WakeFromInterrupt(); else Host.Wake();
        return true;
    }
    CapabilityReadiness Readiness(const ThreadCycleContext&) const noexcept { return {Pending.load(),{},true,false}; }
    void Service(const ThreadCycleContext&) { if (Pending.exchange(false)) ++Serviced; }
    void Quiesce(const ThreadCycleContext&) noexcept { if (Pending.exchange(false)) ++Discarded; }
};
class NoticeWorker final : public ThreadWith<Notice> {
public:
    Notice& Inbox() { return GetCapability<NoticeTag>(); }
    ~NoticeWorker() override { assert(Shutdown()==ThreadStatus::Success); }
};
struct DeadlineA {}; struct DeadlineB {};
template<class Tag> struct DeadlineCapability final : ThreadCapability {
    using CapabilityTag=Tag;
    static constexpr bool NeedsMonotonicTime=true;
    mutable System::Synchronization::Mutex Mutex;
    ThreadHostServices Host;
    std::atomic<bool> HasDeadline{false};
    std::atomic<std::uint64_t> Deadline{0};
    std::atomic<unsigned> Serviced{0};
    ThreadStatus Initialize(const ThreadHostServices& host) { std::lock_guard<System::Synchronization::Mutex> lock(Mutex); Host=host; return ThreadStatus::Success; }
    void Set(ThreadDeadline value) {
        { std::lock_guard<System::Synchronization::Mutex> lock(Mutex);
          Deadline.store(value.MonotonicNanoseconds); HasDeadline.store(value.Present); }
        Host.Wake();
    }
    CapabilityReadiness Readiness(const ThreadCycleContext&) const noexcept {
        if (!HasDeadline.load()) return {};
        // Immediate stays false: the neutral host must recognize a due asynchronous deadline itself.
        return {false,ThreadDeadline::At(Deadline.load()),true,false};
    }
    void Service(const ThreadCycleContext& context) {
        std::lock_guard<System::Synchronization::Mutex> lock(Mutex);
        if (HasDeadline.load() && Deadline.load()<=context.Now) { HasDeadline=false; ++Serviced; }
    }
};
class DeadlineWorker final : public ThreadWith<DeadlineCapability<DeadlineA>,DeadlineCapability<DeadlineB>> {
public:
    explicit DeadlineWorker(ManualClock& clock):ThreadWith({},&clock) {}
    auto& A() { return GetCapability<DeadlineA>(); }
    auto& B() { return GetCapability<DeadlineB>(); }
    ~DeadlineWorker() override { assert(Shutdown()==ThreadStatus::Success); }
};
int main() {
    HostRuntime runtime; CountSignals signals(runtime);
    {
        NoticeWorker worker;
        assert(worker.Initialize()==ThreadStatus::Success);
        // Work arrives before Start/Wait; all repeated wake notifications coalesce.
        assert(worker.Inbox().TryPublish(true));
        for (unsigned i=0;i<100;++i) worker.Wake();
        assert(worker.Start()==ThreadStatus::Success);
        Await([&]{return worker.Inbox().Serviced==1;});
        std::atomic<unsigned> accepted{1};
        auto producer=[&](bool interrupt) {
            for (unsigned i=0;i<20000;++i) {
                if (worker.Inbox().TryPublish(interrupt)) ++accepted;
                if ((i%7)==0) std::this_thread::yield();
            }
        };
        std::thread first(producer,false),second(producer,true);
        first.join(); second.join();
        Await([&]{return worker.Inbox().Serviced==accepted;});
        assert(worker.Pause()==ThreadStatus::Success);
        Await([&]{return worker.Inbox().TryPublish(true);});
        for (unsigned i=0;i<100;++i) assert(!worker.Inbox().TryPublish());
        const auto before=worker.Inbox().Serviced.load();
        assert(worker.Start()==ThreadStatus::Success);
        Await([&]{return worker.Inbox().Serviced==before+1;});
        assert(worker.Shutdown()==ThreadStatus::Success);
        assert(!worker.Inbox().TryPublish());
    }
    {
        NoticeWorker worker; assert(worker.Initialize()==ThreadStatus::Success);
        ThreadAdmissionClaim claim(worker.Inbox().Host); assert(claim);
        std::atomic<bool> requested{false};
        std::thread terminator([&]{ requested=true; worker.Terminate(); });
        Await([&]{return requested.load();});
        assert(worker.GetThreadState()==ThreadState::Initialized);
        worker.Inbox().Pending=true; claim.Release(); worker.Wake(); terminator.join();
        assert(worker.Shutdown()==ThreadStatus::Success);
        assert(worker.Inbox().Serviced==0 && worker.Inbox().Discarded==1);
        assert(!worker.Inbox().TryPublish());
    }
    assert(signals.Interrupt>0 && signals.Ordinary>0 && signals.Resets==0);
    {
        ManualClock clock;
        DeadlineWorker worker(clock);
        assert(worker.Initialize()==ThreadStatus::Success);
        worker.A().Set(ThreadDeadline::At(5000000)); worker.B().Set(ThreadDeadline::At(2000000));
        assert(worker.Start()==ThreadStatus::Success);
        Await([&]{return signals.LastTimeout==2;});
        clock.Value=2000000; worker.Wake(); Await([&]{return worker.B().Serviced==1;});
        assert(worker.A().Serviced==0);
        Await([&]{return signals.LastTimeout==3;});
        worker.A().Set({}); Await([&]{return signals.LastTimeout==UINT32_MAX;});
        worker.A().Set(ThreadDeadline::At(UINT64_MAX)); Await([&]{return signals.LastTimeout==UINT32_MAX-1;});
        // NoDeadline and an absolute UINT64_MAX deadline are observably distinct.
        worker.A().Set(ThreadDeadline::At(2000000)); Await([&]{return worker.A().Serviced==1;});
        assert(worker.Shutdown()==ThreadStatus::Success);
    }
    assert(runtime.Created==runtime.Joined);
}
