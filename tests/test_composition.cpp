#include "TestRuntime.hpp"
#include <ESPressio_ThreadWith.hpp>
#include <ESPressio_Precision.hpp>
#include <array>
#include <cassert>
#include <new>
using namespace ESPressio::Threads;
static std::atomic<bool> denyHeap{false};
void* operator new(std::size_t n) { if (denyHeap) std::abort(); if (void* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
struct A {}; struct B {};
struct Trace {
    std::array<char,128> Values{};
    std::atomic<unsigned> Count{0},Loops{0};
    ManualClock Clock;
    void Add(char value) { const auto i=Count.fetch_add(1); assert(i<Values.size()); Values[i]=value; }
};
template<class Tag,char Marker> struct CounterCapability final : ThreadCapability {
    using CapabilityTag=Tag;
    static constexpr bool NeedsMonotonicTime=true;
    static constexpr std::uint32_t FrameworkStackFloorBytes=2048;
    std::atomic<unsigned> Pending{0},Serviced{0},Discarded{0},Rollbacks{0},Quiesced{0};
    ThreadHostServices Host;
    Trace* Output=nullptr;
    bool Frozen=false,FailInitialize=false,FailActivate=false,FailService=false,FailQuiesce=false;
    bool Bind(Trace& trace) { if (Frozen) return false; Output=&trace; return true; }
    ThreadStatus Initialize(const ThreadHostServices& host) { Host=host; return FailInitialize ? ThreadStatus::StorageUnavailable : ThreadStatus::Success; }
    ThreadStatus FinalizeInitialization() { Frozen=true; return Output ? ThreadStatus::Success : ThreadStatus::InvalidConfiguration; }
    void RollbackInitialization() noexcept { ++Rollbacks; Frozen=false; Host={}; }
    void Activate(const ThreadCycleContext&) { if (FailActivate) throw 31; }
    CapabilityReadiness Readiness(const ThreadCycleContext&) const noexcept { return {Pending.load()!=0,{},true,false}; }
    void Service(const ThreadCycleContext&) {
        assert(Pending.fetch_sub(1)>0); ++Serviced; Output->Add(Marker);
        Output->Clock.Value.fetch_add(50);
        if (FailService) throw 41;
    }
    void Quiesce(const ThreadCycleContext&) { Discarded.fetch_add(Pending.exchange(0)); ++Quiesced; Frozen=false; if (FailQuiesce) throw 51; }
    void Admit(unsigned count) { Pending.fetch_add(count); Host.Wake(); }
};
using CA=CounterCapability<A,'A'>; using CB=CounterCapability<B,'B'>;
template<class First,class Last> class Worker final : public ThreadWith<First,Precision<4>,Last> {
public:
    Trace& Output;
    explicit Worker(Trace& trace):ThreadWith<First,Precision<4>,Last>({},&trace.Clock),Output(trace) {
        this->template GetCapability<A>().Bind(trace); this->template GetCapability<B>().Bind(trace);
        this->template GetCapability<PrecisionTag>().SetCadencePeriod(100);
    }
    ~Worker() override { assert(this->Shutdown()==ThreadStatus::Success); }
    CA& FirstCounter() { return this->template GetCapability<A>(); }
    CB& SecondCounter() { return this->template GetCapability<B>(); }
protected:
    ThreadWorkDisposition OnLoop() override { Output.Add('P'); ++Output.Loops; return ThreadWorkDisposition::IdleReady; }
};
template<class First,class Last> void Fairness(HostRuntime& runtime) {
    Trace trace; Worker<First,Last> worker(trace);
    static_assert(Worker<First,Last>::template HasCapability<A>());
    static_assert(!Worker<First,Last>::template HasCapability<int>());
    const auto profile=GetThreadResourceProfile(worker);
    assert(profile.ResidentBytes==sizeof(worker) && profile.FrameworkStackFloorBytes==2048);
    const auto tasks=runtime.Created.load(),signals=runtime.Signals.load();
    assert(worker.Initialize()==ThreadStatus::Success && runtime.Created==tasks+1 && runtime.Signals==signals+1);
    assert(!worker.FirstCounter().Bind(trace));
    denyHeap=true;
    worker.FirstCounter().Admit(6); worker.SecondCounter().Admit(6);
    assert(worker.Start()==ThreadStatus::Success);
    Await([&]{return trace.Loops==7;});
    assert(worker.Shutdown()==ThreadStatus::Success);
    denyHeap=false;
    assert(trace.Count==19 && worker.FirstCounter().Serviced==6 && worker.SecondCounter().Serviced==6);
    unsigned a=0,b=0;
    for (unsigned i=0;i<trace.Count;++i) {
        if (i%3==0) assert(trace.Values[i]=='P'); // A due cadence runs before the next async quantum.
        else { if (trace.Values[i]=='A') ++a; else { assert(trace.Values[i]=='B'); ++b; }
            assert(a<=b+1 && b<=a+1); }
    }
}
int main() {
    HostRuntime runtime;
    Fairness<CA,CB>(runtime); Fairness<CB,CA>(runtime);
    {
        Trace trace; Worker<CA,CB> worker(trace);
        worker.SecondCounter().FailInitialize=true;
        assert(worker.Initialize()==ThreadStatus::StorageUnavailable);
        assert(worker.FirstCounter().Rollbacks==1 && worker.SecondCounter().Rollbacks==1);
        worker.SecondCounter().FailInitialize=false;
        assert(worker.Initialize()==ThreadStatus::Success);
        worker.FirstCounter().Admit(3); worker.SecondCounter().Admit(3);
        worker.FirstCounter().FailService=true; worker.FirstCounter().FailQuiesce=true;
        assert(worker.Start()==ThreadStatus::Success);
        Await([&]{return worker.GetThreadState()==ThreadState::Terminated;});
        assert(worker.GetDiagnostics().Failure.Phase==ThreadFailurePhase::Capability);
        assert(worker.FirstCounter().Quiesced==1 && worker.SecondCounter().Quiesced==1);
        assert(worker.FirstCounter().Serviced+worker.FirstCounter().Discarded==3);
        assert(worker.SecondCounter().Serviced+worker.SecondCounter().Discarded==3);
        assert(worker.Shutdown()==ThreadStatus::Success);
    }
    {
        Trace trace; Worker<CA,CB> worker(trace); worker.SecondCounter().FailActivate=true;
        assert(worker.Initialize()==ThreadStatus::Success && worker.Start()==ThreadStatus::Success);
        Await([&]{return worker.GetThreadState()==ThreadState::Terminated;});
        assert(worker.GetDiagnostics().Failure.Phase==ThreadFailurePhase::Lifecycle && trace.Loops==0);
        assert(worker.Shutdown()==ThreadStatus::Success);
    }
    assert(runtime.Created==runtime.Joined);
}
