#pragma once
#include "ESPressio_ThreadTypes.hpp"
namespace ESPressio::Threads {
/// <summary>Minimal physical lifecycle and diagnostics; ownership always remains with the caller.</summary>
class IThread {
public:
    virtual ~IThread()=default;
    virtual ThreadStatus Initialize()=0;
    virtual ThreadStatus Start()=0;
    virtual ThreadStatus Pause()=0;
    virtual void Terminate() noexcept=0;
    virtual ThreadStatus Shutdown() noexcept=0;
    virtual ThreadState GetThreadState() const noexcept=0;
    virtual ThreadDiagnostics GetDiagnostics() const=0;
};
}
