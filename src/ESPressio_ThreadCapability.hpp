#pragma once
#include <type_traits>
#include <utility>
#include "ESPressio_ThreadTypes.hpp"
namespace ESPressio::Threads {
/// <summary>Borrowed neutral services. Publish domain state before Wake; no service owns another execution context.</summary>
struct ThreadHostServices final {
    void* Owner=nullptr;
    bool (*WakeFunction)(void*,bool) noexcept=nullptr;
    bool (*AcceptingFunction)(const void*) noexcept=nullptr;
    std::uint64_t (*NowFunction)(const void*) noexcept=nullptr;
    bool (*BeginAdmissionFunction)(void*) noexcept=nullptr;
    void (*EndAdmissionFunction)(void*) noexcept=nullptr;
    bool Wake() const noexcept { return WakeFunction && WakeFunction(Owner,false); }
    bool WakeFromInterrupt() const noexcept { return WakeFunction && WakeFunction(Owner,true); }
    bool IsAccepting() const noexcept { return AcceptingFunction && AcceptingFunction(Owner); }
    std::uint64_t Now() const noexcept { return NowFunction ? NowFunction(Owner) : 0; }
};
/// <summary>Nonblocking ordinary-context admission claim, linearized against root termination.</summary>
/// <remarks>Hold only through bounded domain publication; release before waking/calling other code. Not an ISR API.</remarks>
class ThreadAdmissionClaim final {
    const ThreadHostServices& _host;
    bool _held;
public:
    explicit ThreadAdmissionClaim(const ThreadHostServices& host) noexcept :_host(host),
        _held(host.BeginAdmissionFunction && host.EndAdmissionFunction && host.BeginAdmissionFunction(host.Owner)) {}
    ThreadAdmissionClaim(const ThreadAdmissionClaim&)=delete;
    ThreadAdmissionClaim& operator=(const ThreadAdmissionClaim&)=delete;
    ~ThreadAdmissionClaim() { Release(); }
    explicit operator bool() const noexcept { return _held; }
    void Release() noexcept { if (_held) { _held=false; _host.EndAdmissionFunction(_host.Owner); } }
};
struct ThreadCycleContext final { std::uint64_t Now=0; ThreadHostServices Services{}; };
/// <summary>Static capability readiness is a cheap, side-effect-free predicate; a future deadline is distinct.</summary>
struct CapabilityReadiness final {
    bool Immediate=false;
    ThreadDeadline Deadline{};
    bool ApplicationEligible=true;
    bool ApplicationDeadlineDue=false;
};
/// <summary>Neutral exclusive claims are Types, not runtime names or registries.</summary>
template<class... Roles> struct CapabilityClaims {};
struct ApplicationCadenceRole final {};
/// <summary>Defaults for the neutral static protocol. Concrete capabilities must declare their own CapabilityTag.</summary>
struct ThreadCapability {
    using ExclusiveClaims=CapabilityClaims<>;
    static constexpr std::uint32_t FrameworkStackFloorBytes=0;
    static constexpr std::size_t ExternalStorageBytes=0,ExternalStorageAlignment=1;
    static constexpr bool NeedsMonotonicTime=false;
    ThreadStatus Initialize(const ThreadHostServices&) { return ThreadStatus::Success; }
    ThreadStatus FinalizeInitialization() { return ThreadStatus::Success; }
    void RollbackInitialization() noexcept {}
    void Activate(const ThreadCycleContext&) {}
    void Pause(const ThreadCycleContext&) {}
    void Quiesce(const ThreadCycleContext&) noexcept {}
    CapabilityReadiness Readiness(const ThreadCycleContext&) const noexcept { return {}; }
    void Service(const ThreadCycleContext&) {}
    bool BeforeApplication(const ThreadCycleContext&) { return true; }
    void AfterApplication(const ThreadCycleContext&) {}
};
enum class ThreadHostOperation : std::uint8_t {
    Initialize, FinalizeInitialization, RollbackInitialization, Activate, Pause,
    Quiesce, Inspect, Service, BeforeApplication, AfterApplication, Resources
};
/// <summary>The single root/host bridge has only neutral lifecycle, readiness and resource vocabulary.</summary>
struct ThreadHostResult final {
    ThreadStatus Status=ThreadStatus::Success;
    CapabilityReadiness Ready{};
    ThreadResourceProfile Resources{};
    bool NeedsMonotonicTime=false;
};
namespace Internal {
template<class C,class=void> struct ValidCapability : std::false_type {};
template<class C> struct ValidCapability<C,std::void_t<typename C::CapabilityTag,typename C::ExclusiveClaims,
    decltype(C::FrameworkStackFloorBytes),decltype(C::ExternalStorageBytes),decltype(C::ExternalStorageAlignment),
    decltype(C::NeedsMonotonicTime),
    decltype(std::declval<C&>().Initialize(std::declval<const ThreadHostServices&>())),
    decltype(std::declval<C&>().FinalizeInitialization()),decltype(std::declval<C&>().RollbackInitialization()),
    decltype(std::declval<C&>().Activate(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<C&>().Pause(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<C&>().Quiesce(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<const C&>().Readiness(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<C&>().Service(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<C&>().BeforeApplication(std::declval<const ThreadCycleContext&>())),
    decltype(std::declval<C&>().AfterApplication(std::declval<const ThreadCycleContext&>()))>>
    : std::bool_constant<std::is_default_constructible_v<C> && std::is_nothrow_destructible_v<C> &&
        std::is_same_v<decltype(std::declval<C&>().Initialize(std::declval<const ThreadHostServices&>())),ThreadStatus> &&
        std::is_same_v<decltype(std::declval<C&>().FinalizeInitialization()),ThreadStatus> &&
        std::is_same_v<decltype(std::declval<const C&>().Readiness(std::declval<const ThreadCycleContext&>())),CapabilityReadiness> &&
        std::is_same_v<decltype(std::declval<C&>().BeforeApplication(std::declval<const ThreadCycleContext&>())),bool> &&
        noexcept(std::declval<const C&>().Readiness(std::declval<const ThreadCycleContext&>()))> {};
template<class A,class B> struct ClaimsDisjoint;
template<class Role,class... Other> constexpr bool RoleAbsent=( !std::is_same_v<Role,Other> && ... );
template<class... A,class... B> struct ClaimsDisjoint<CapabilityClaims<A...>,CapabilityClaims<B...>>
    : std::bool_constant<(RoleAbsent<A,B...> && ...)> {};
template<class... C> constexpr bool ExternalStorageTotalFits() noexcept {
    std::size_t total=0; bool fits=true;
    auto add=[&](std::size_t bytes) constexpr {
        if (bytes>SIZE_MAX-total) fits=false;
        else total+=bytes;
    };
    (add(C::ExternalStorageBytes),...);
    return fits;
}
template<class... C> struct DistinctCapabilities : std::true_type {};
template<class First,class... Rest> struct DistinctCapabilities<First,Rest...> :
    std::bool_constant<(!std::is_same_v<typename First::CapabilityTag,typename Rest::CapabilityTag> && ...) &&
        (ClaimsDisjoint<typename First::ExclusiveClaims,typename Rest::ExclusiveClaims>::value && ...) &&
        DistinctCapabilities<Rest...>::value> {};
}
}
