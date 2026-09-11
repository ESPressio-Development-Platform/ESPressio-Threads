#pragma once
#include <array>
#include <tuple>
#include <type_traits>
#include "ESPressio_Thread.hpp"
namespace ESPressio::Threads {
namespace Internal {
template<class Tag,class First,class... Rest> constexpr std::size_t CapabilityIndex() {
    if constexpr (std::is_same_v<Tag,typename First::CapabilityTag>) return 0;
    else { static_assert(sizeof...(Rest)>0,"Requested CapabilityTag is not installed");
        if constexpr (sizeof...(Rest)>0) return 1+CapabilityIndex<Tag,Rest...>(); else return 0; }
}
}
/// <summary>The sole nonempty static capability host; declaration order has no semantic priority.</summary>
template<class... Capabilities> class ThreadWith : public Thread {
    static_assert(sizeof...(Capabilities)>0,"Use bare Thread for zero capabilities; ThreadWith must be nonempty");
    static_assert((Internal::ValidCapability<Capabilities>::value && ...),"Invalid Thread capability protocol; inspect the offending descriptor Type");
    static_assert((!std::is_base_of_v<Thread,Capabilities> && ...),"Capabilities must not inherit a physical Thread");
    static_assert(Internal::DistinctCapabilities<Capabilities...>::value,"Duplicate CapabilityTag or conflicting exclusive role claims");
    static_assert(((Capabilities::ExternalStorageAlignment>0 &&
        (Capabilities::ExternalStorageAlignment & (Capabilities::ExternalStorageAlignment-1))==0) && ...),"Invalid capability storage alignment");
    static_assert(Internal::ExternalStorageTotalFits<Capabilities...>(),"Capability external storage total overflows size_t");
    std::tuple<Capabilities...> _capabilities{};
    std::size_t _initialized=0,_cursor=0;
    bool _frozen=false;
    template<class Function> void Each(Function&& function) {
        std::apply([&](auto&... capability){ (function(capability),...); },_capabilities);
    }
    template<std::size_t Index=sizeof...(Capabilities)> void Rollback(std::exception_ptr& first) {
        if constexpr (Index>0) {
            if (Index<=_initialized) { try { std::get<Index-1>(_capabilities).RollbackInitialization(); }
                catch (...) { if (!first) first=std::current_exception(); } }
            Rollback<Index-1>(first);
        }
    }
    CapabilityReadiness Inspect(const ThreadCycleContext& context) const noexcept {
        CapabilityReadiness result;
        std::apply([&](const auto&... capability) {
            auto merge=[&](const auto& value) {
                const auto ready=value.Readiness(context);
                result.Immediate=result.Immediate || ready.Immediate ||
                    (ready.Deadline.Due(context.Now) && !ready.ApplicationDeadlineDue);
                result.Deadline.Include(ready.Deadline);
                result.ApplicationEligible=result.ApplicationEligible && ready.ApplicationEligible;
                result.ApplicationDeadlineDue=result.ApplicationDeadlineDue || ready.ApplicationDeadlineDue;
            };
            (merge(capability),...);
        },_capabilities);
        return result;
    }
    void ServiceOne(const ThreadCycleContext& context) {
        constexpr auto count=sizeof...(Capabilities);
        if constexpr (count>0) {
            const auto ready=[&] {
                std::array<bool,count> values{}; std::size_t i=0;
                Each([&](auto& capability) { const auto value=capability.Readiness(context);
                    values[i++]=value.Immediate || (value.Deadline.Due(context.Now) && !value.ApplicationDeadlineDue); });
                return values;
            }();
            for (std::size_t offset=0;offset<count;++offset) {
                const auto selected=(_cursor+offset)%count;
                if (!ready[selected]) continue;
                _cursor=(selected+1)%count;
                std::size_t i=0;
                Each([&](auto& capability) { if (i++==selected) capability.Service(context); });
                return;
            }
        }
    }
    ThreadHostResult Host(ThreadHostOperation operation,ThreadCycleContext& context) final override {
        ThreadHostResult result;
        switch (operation) {
        case ThreadHostOperation::Resources:
            result.Resources.ResidentBytes=sizeof(*this); result.Resources.ResidentAlignment=alignof(ThreadWith);
            result.NeedsMonotonicTime=(Capabilities::NeedsMonotonicTime || ...);
            Each([&](auto& capability) {
                using C=std::remove_reference_t<decltype(capability)>;
                result.Resources.FrameworkStackFloorBytes=std::max(result.Resources.FrameworkStackFloorBytes,C::FrameworkStackFloorBytes);
                result.Resources.ExternalStorageBytes+=C::ExternalStorageBytes;
                result.Resources.ExternalStorageAlignment=std::max(result.Resources.ExternalStorageAlignment,C::ExternalStorageAlignment);
            });
            break;
        case ThreadHostOperation::Initialize:
            _initialized=0; _cursor=0; _frozen=false;
            Each([&](auto& capability) {
                if (result.Status==ThreadStatus::Success) { ++_initialized; result.Status=capability.Initialize(context.Services); }
            });
            break;
        case ThreadHostOperation::FinalizeInitialization:
            Each([&](auto& capability) { if (result.Status==ThreadStatus::Success) result.Status=capability.FinalizeInitialization(); });
            _frozen=result.Status==ThreadStatus::Success;
            break;
        case ThreadHostOperation::RollbackInitialization: {
            std::exception_ptr cause; Rollback(cause); _initialized=0; _frozen=false;
            if (cause) std::rethrow_exception(cause);
            break;
        }
        case ThreadHostOperation::Activate:
            if (!_frozen) throw ThreadStatus::InvalidState;
            Each([&](auto& capability){ capability.Activate(context); }); break;
        case ThreadHostOperation::Pause:
            Each([&](auto& capability){ capability.Pause(context); }); break;
        case ThreadHostOperation::Quiesce: {
            std::exception_ptr first;
            Each([&](auto& capability) { try { capability.Quiesce(context); }
                catch (...) { if (!first) first=std::current_exception(); } });
            _frozen=false;
            if (first) std::rethrow_exception(first);
            break;
        }
        case ThreadHostOperation::Inspect: result.Ready=Inspect(context); break;
        case ThreadHostOperation::Service: ServiceOne(context); break;
        case ThreadHostOperation::BeforeApplication:
            result.Ready=Inspect(context);
            if (result.Ready.ApplicationEligible) Each([&](auto& capability){
                if (result.Ready.ApplicationEligible) result.Ready.ApplicationEligible=capability.BeforeApplication(context);
            });
            break;
        case ThreadHostOperation::AfterApplication:
            Each([&](auto& capability){ capability.AfterApplication(context); }); break;
        }
        return result;
    }
protected:
    template<class Tag> decltype(auto) GetCapability() noexcept {
        static_assert(HasCapability<Tag>(),"Requested CapabilityTag is not installed");
        return std::get<Internal::CapabilityIndex<Tag,Capabilities...>()>(_capabilities);
    }
    template<class Tag> decltype(auto) GetCapability() const noexcept {
        static_assert(HasCapability<Tag>(),"Requested CapabilityTag is not installed");
        return std::get<Internal::CapabilityIndex<Tag,Capabilities...>()>(_capabilities);
    }
public:
    using Thread::Thread;
    template<class Tag> static constexpr bool HasCapability() noexcept {
        return (std::is_same_v<Tag,typename Capabilities::CapabilityTag> || ...);
    }
    ~ThreadWith() override { if (this->Shutdown()!=ThreadStatus::Success) std::terminate(); }
};
/// <summary>Exact concrete C++ object accounting without RTTI or a runtime registry.</summary>
template<class Concrete> ThreadResourceProfile GetThreadResourceProfile(Concrete& thread) {
    static_assert(std::is_base_of_v<Thread,Concrete>,"Resource description requires a concrete Thread owner");
    auto profile=thread.GetResourceProfile(); profile.ResidentBytes=sizeof(Concrete); profile.ResidentAlignment=alignof(Concrete);
    return profile;
}
}
