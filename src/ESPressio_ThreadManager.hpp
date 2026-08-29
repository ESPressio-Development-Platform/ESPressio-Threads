#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

#include <algorithm>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

#include <ESPressio_Execution.hpp>
#include <ESPressio_Memory.hpp>
#include "ESPressio_ThreadSafe.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"
#include "ESPressio_IThreadManagerObserver.hpp"
#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

/// <summary>Captures the initialization outcome for one registered thread.</summary>
struct ThreadInitializationResult {
    /// <summary>Identifier of the thread that was initialized.</summary>
    uint8_t threadID;
    /// <summary>Status returned by the thread's initialization attempt.</summary>
    ThreadInitializationStatus status;
};

/// <summary>Owns the registry and lifecycle coordination for ESPressio threads.</summary>
/// <remarks>The manager assigns cores, initializes registered threads, coordinates automatic cleanup, and publishes manager-level lifecycle observations. Variable-size registry, snapshot, and result storage prefers external memory, while optional observer bookkeeping is materialized only when first requested.</remarks>
class ThreadManager {
private:
    static constexpr auto ExternalPreferred =
        System::Memory::MemoryPolicy::ExternalPreferred;

    class ManagerObservable final : public Observable::ThreadSafeObservable {
        template<typename TCallback>
        void NotifyObservers(TCallback&& callback) {
            ExecuteNotification([&](NotificationContext& notification) {
                notification.WithObservers<IThreadManagerObserver>(
                    [&](IThreadManagerObserver* observer) {
                        try { callback(observer); } catch (...) {}
                    }
                );
            });
        }
    public:
        void ThreadRegistered(IThread* thread, const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadRegistered(thread, snapshot); });
        }
        void ThreadRegistrationFailed(IThread* thread, std::exception_ptr cause) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadRegistrationFailed(thread, cause); });
        }
        void ThreadRemoved(const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadRemoved(snapshot); });
        }
        void CleanupClaimed(IThread* thread, const ThreadManagerThreadSnapshot& snapshot) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadCleanupClaimed(thread, snapshot); });
        }
        void CleanupDeferred(const ThreadManagerCleanupResult& result) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadCleanupDeferred(result); });
        }
        void CleanupStarted(const ThreadManagerCleanupResult& result) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadCleanupStarted(result); });
        }
        void CleanupCompleted(const ThreadManagerCleanupResult& result) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadCleanupCompleted(result); });
        }
        void CleanupFailed(const ThreadManagerCleanupResult& result, std::exception_ptr cause) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadCleanupFailed(result, cause); });
        }
        void InitializationCompleted(const ThreadManagerInitializationResult& result) {
            NotifyObservers([&](IThreadManagerObserver* o){ o->OnThreadManagerInitializationCompleted(result); });
        }
    };

    struct ThreadRecord {
        uint8_t id;
        IThread* thread;
        int coreID;
        bool operator==(const ThreadRecord& other) const {
            return id == other.id && thread == other.thread && coreID == other.coreID;
        }
    };

    struct ThreadInitializationTarget {
        IThread* thread;
        uint8_t id;
    };

    struct ClaimedCleanupRecord {
        uint8_t id;
        IThread* thread;
        ThreadManagerThreadSnapshot snapshot;
        bool removed;
    };

    using ThreadRecordStorage = System::Memory::Vector<
        ThreadRecord,
        ExternalPreferred
    >;
    using ThreadPointerSnapshot = System::Memory::Vector<
        IThread*,
        ExternalPreferred
    >;
    using CleanupRecordStorage = System::Memory::Vector<
        ClaimedCleanupRecord,
        ExternalPreferred
    >;
    using InitializationTargetStorage = System::Memory::Vector<
        ThreadInitializationTarget,
        ExternalPreferred
    >;

public:
    /// <summary>Externally preferred owning storage returned by <c>InitializeWithResults()</c>.</summary>
    using InitializationResultStorage = System::Memory::Vector<
        ThreadInitializationResult,
        ExternalPreferred
    >;

private:
    ReadWriteMutex<ThreadRecordStorage> _threads;
    ReadWriteMutex<int> _nextCoreID = ReadWriteMutex<int>(0);
    std::recursive_mutex _iterationMutex;
    std::size_t _activeIterations = 0;
    bool _cleanupPending = false;
    mutable std::mutex _observableMutex;
    std::shared_ptr<ManagerObservable> _observable;

    static ThreadManagerThreadSnapshot _snapshot(const ThreadRecord& record) {
        ThreadManagerThreadSnapshot snapshot;
        snapshot.ThreadID = record.id;
        snapshot.CoreID = record.coreID;
        if (record.thread != nullptr) {
            try { snapshot.State = record.thread->GetThreadState(); } catch (...) {}
            try { snapshot.FreeOnTerminate = record.thread->GetFreeOnTerminate(); } catch (...) {}
            try { snapshot.StartOnInitialize = record.thread->GetStartOnInitialize(); } catch (...) {}
        }
        return snapshot;
    }

    std::shared_ptr<ManagerObservable> ObservableSnapshot() const {
        std::lock_guard<std::mutex> lock(_observableMutex);
        return _observable;
    }

    std::shared_ptr<ManagerObservable> EnsureObservable() noexcept {
        std::lock_guard<std::mutex> lock(_observableMutex);
        if (_observable) return _observable;
        try {
            _observable = System::Memory::MakeShared<
                ManagerObservable,
                ExternalPreferred
            >();
        } catch (...) {
            return {};
        }
        return _observable;
    }

    template<typename TNotification>
    void NotifyIfObserved(TNotification&& notification) {
        auto observable = ObservableSnapshot();
        if (observable) notification(*observable);
    }

    void _beginIteration() {
        std::lock_guard<std::recursive_mutex> lock(_iterationMutex);
        ++_activeIterations;
    }

    void _endIteration() {
        bool runDeferredCleanup = false;
        {
            std::lock_guard<std::recursive_mutex> lock(_iterationMutex);
            if (_activeIterations > 0) --_activeIterations;
            if (_activeIterations == 0 && _cleanupPending) {
                _cleanupPending = false;
                runDeferredCleanup = true;
            }
        }
        if (runDeferredCleanup) CleanUp();
    }

    class IterationGuard {
        ThreadManager& _manager;
    public:
        explicit IterationGuard(ThreadManager& manager) : _manager(manager) { _manager._beginIteration(); }
        ~IterationGuard() { _manager._endIteration(); }
        IterationGuard(const IterationGuard&) = delete;
        IterationGuard& operator=(const IterationGuard&) = delete;
    };

    static int _getCoreCount() {
        const uint32_t count = System::Execution::Provider().ProcessorCount();
        return count > 0 ? static_cast<int>(count) : 1;
    }

protected:
    /// <summary>Constructs the singleton manager without allocating dynamic storage.</summary>
    /// <remarks>The registry allocator binds lazily to the active ESPressio System memory provider on first growth; manager observer infrastructure is also deferred until first registration.</remarks>
    ThreadManager()
        : _threads(ThreadRecordStorage{}) {}

public:
    /// <summary>Returns the process-wide thread manager singleton without heap-allocating the manager itself.</summary>
    static ThreadManager* GetInstance() {
        static ThreadManager instance;
        return &instance;
    }

    /// <summary>Registers a thread and assigns it to a processor core.</summary>
    /// <param name="thread">Thread instance to register.</param>
    /// <param name="assignedThreadID">Optional output receiving an automatically allocated thread identifier.</param>
    /// <returns>The processor core assigned to the thread.</returns>
    /// <remarks>If no identifier output is supplied, the thread's existing identifier must be unique. Registration failures are observed and then rethrown.</remarks>
    int AddThread(IThread* thread, uint8_t* assignedThreadID = nullptr) {
        try {
            if (thread == nullptr) throw ThreadInvalidRegistrationException();
            ThreadRecord resolved{0, nullptr, 0};

            _threads.WithSharedReadLock([&](const auto& threads) {
                const auto existing = std::find_if(
                    threads.begin(), threads.end(),
                    [thread](const ThreadRecord& record){ return record.thread == thread; }
                );
                if (existing != threads.end()) resolved = *existing;
            });

            if (resolved.thread != nullptr) {
                if (assignedThreadID != nullptr) *assignedThreadID = resolved.id;
                return resolved.coreID;
            }

            bool inserted = false;
            const uint8_t requestedThreadID =
                assignedThreadID == nullptr ? thread->GetThreadID() : 0;

            _threads.WithWriteLock([&](auto& threads) {
                const auto existing = std::find_if(
                    threads.begin(), threads.end(),
                    [thread](const ThreadRecord& record){ return record.thread == thread; }
                );
                if (existing != threads.end()) {
                    resolved = *existing;
                    return;
                }

                uint8_t recordID = requestedThreadID;
                if (assignedThreadID != nullptr) {
                    bool assigned = false;
                    for (unsigned int candidate = 1;
                         candidate <= std::numeric_limits<uint8_t>::max(); ++candidate) {
                        const uint8_t candidateID = static_cast<uint8_t>(candidate);
                        const bool inUse = std::any_of(
                            threads.begin(), threads.end(),
                            [candidateID](const ThreadRecord& record){ return record.id == candidateID; }
                        );
                        if (!inUse) { recordID = candidateID; assigned = true; break; }
                    }
                    if (!assigned) {
                        const bool zeroInUse = std::any_of(
                            threads.begin(), threads.end(),
                            [](const ThreadRecord& record){ return record.id == 0; }
                        );
                        if (!zeroInUse) { recordID = 0; assigned = true; }
                    }
                    if (!assigned) throw ThreadLimitExceededException();
                } else {
                    const bool idInUse = std::any_of(
                        threads.begin(), threads.end(),
                        [recordID](const ThreadRecord& record){ return record.id == recordID; }
                    );
                    if (idInUse) throw ThreadDuplicateIDException(recordID);
                }

                int useCore = 0;
                _nextCoreID.WithWriteLock([&](int& nextCoreID) {
                    const int coreCount = _getCoreCount();
                    useCore = nextCoreID % coreCount;
                    threads.push_back({recordID, thread, useCore});
                    nextCoreID = (useCore + 1) % coreCount;
                });
                resolved = {recordID, thread, useCore};
                inserted = true;
            });

            if (assignedThreadID != nullptr) *assignedThreadID = resolved.id;
            if (inserted) {
                NotifyIfObserved([&](ManagerObservable& observable) {
                    observable.ThreadRegistered(thread, _snapshot(resolved));
                });
            }
            return resolved.coreID;
        } catch (...) {
            auto cause = std::current_exception();
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.ThreadRegistrationFailed(thread, cause);
            });
            throw;
        }
    }

    /// <summary>Removes the specified thread instance from the registry without deleting it.</summary>
    void RemoveThread(IThread* thread) {
        bool removed = false;
        ThreadRecord removedRecord{0, nullptr, 0};
        _threads.WithWriteLock([&](auto& threads) {
            const auto matching = std::find_if(
                threads.begin(), threads.end(),
                [thread](const ThreadRecord& record){ return record.thread == thread; }
            );
            if (matching == threads.end()) return;
            removedRecord = *matching;
            threads.erase(matching);
            removed = true;
        });
        if (removed) {
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.ThreadRemoved(_snapshot(removedRecord));
            });
        }
    }

    /// <summary>Removes the thread with the specified identifier from the registry without deleting it.</summary>
    void RemoveThread(uint8_t threadID) {
        bool removed = false;
        ThreadRecord removedRecord{0, nullptr, 0};
        _threads.WithWriteLock([&](auto& threads) {
            const auto matching = std::find_if(
                threads.begin(), threads.end(),
                [threadID](const ThreadRecord& record){ return record.id == threadID; }
            );
            if (matching == threads.end()) return;
            removedRecord = *matching;
            threads.erase(matching);
            removed = true;
        });
        if (removed) {
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.ThreadRemoved(_snapshot(removedRecord));
            });
        }
    }

    /// <summary>Invokes a callback for a stable externally backed snapshot of all currently registered threads.</summary>
    /// <typeparam name="TCallback">Concrete callable accepting an <c>IThread*</c>.</typeparam>
    /// <remarks>The callable is invoked directly without <c>std::function</c> type erasure or associated callable allocation.</remarks>
    template<typename TCallback>
    void ForEachThread(TCallback&& callback) {
        IterationGuard iteration(*this);
        ThreadPointerSnapshot snapshot;
        _threads.WithSharedReadLock([&](const auto& threads) {
            snapshot.reserve(threads.size());
            for (const ThreadRecord& record : threads) snapshot.push_back(record.thread);
        });
        for (IThread* thread : snapshot) callback(thread);
    }

    /// <summary>Invokes a concrete callback for the thread with the requested identifier when present.</summary>
    /// <typeparam name="TCallback">Concrete callable accepting an <c>IThread*</c>.</typeparam>
    /// <returns><c>true</c> when a matching thread was found and the callback was invoked.</returns>
    /// <remarks>The callback is not type-erased through <c>std::function</c>.</remarks>
    template<typename TCallback>
    bool WithThread(uint8_t threadID, TCallback&& callback) {
        IterationGuard iteration(*this);
        IThread* result = nullptr;
        _threads.WithSharedReadLock([&](const auto& threads) {
            for (const ThreadRecord& record : threads) {
                if (record.id == threadID) { result = record.thread; break; }
            }
        });
        if (result == nullptr) return false;
        callback(result);
        return true;
    }

    /// <summary>Returns the registered thread with the requested identifier, or <c>nullptr</c> when absent.</summary>
    IThread* GetThread(uint8_t threadID) {
        IThread* result = nullptr;
        _threads.WithSharedReadLock([&](const auto& threads) {
            for (const ThreadRecord& record : threads) {
                if (record.id == threadID) { result = record.thread; break; }
            }
        });
        return result;
    }

    /// <summary>Claims and deletes terminated threads that opted into automatic cleanup.</summary>
    /// <returns>Detailed cleanup counts and deferral information.</returns>
    /// <remarks>Cleanup is deferred while a manager iteration snapshot is active.</remarks>
    ThreadManagerCleanupResult CleanUpWithResult() {
        ThreadManagerCleanupResult result;
        ThreadRecordStorage snapshot;
        CleanupRecordStorage claimedRecords;
        std::unique_lock<std::recursive_mutex> iterationLock(_iterationMutex);

        if (_activeIterations > 0) {
            _cleanupPending = true;
            result.WasDeferred = true;
            result.ActiveIterationCount = _activeIterations;
            result.ThreadCountBefore = GetThreadCount();
            result.ThreadCountAfter = result.ThreadCountBefore;
            iterationLock.unlock();
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.CleanupDeferred(result);
            });
            return result;
        }

        try {
            _threads.WithSharedReadLock([&](const auto& threads) {
                snapshot.reserve(threads.size());
                snapshot.insert(snapshot.end(), threads.begin(), threads.end());
            });
            result.ThreadsExamined = snapshot.size();
            result.ThreadCountBefore = snapshot.size();
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.CleanupStarted(result);
            });

            claimedRecords.reserve(snapshot.size());
            for (const ThreadRecord& record : snapshot) {
                if (record.thread != nullptr &&
                    record.thread->GetThreadState() == ThreadState::Terminated &&
                    record.thread->TryClaimAutomaticCleanup()) {
                    claimedRecords.push_back({record.id, record.thread, _snapshot(record), false});
                    ++result.ThreadsClaimed;
                    NotifyIfObserved([&](ManagerObservable& observable) {
                        observable.CleanupClaimed(record.thread, claimedRecords.back().snapshot);
                    });
                }
            }

            ThreadRecordStorage{}.swap(snapshot);

            _threads.WithWriteLock([&](auto& threads) {
                for (ClaimedCleanupRecord& claimed : claimedRecords) {
                    const auto current = std::find_if(
                        threads.begin(), threads.end(),
                        [&claimed](const ThreadRecord& record) {
                            return record.id == claimed.id && record.thread == claimed.thread;
                        }
                    );
                    if (current == threads.end()) continue;
                    threads.erase(current);
                    claimed.removed = true;
                    ++result.ThreadsRemoved;
                }
                result.ThreadCountAfter = threads.size();
            });

            iterationLock.unlock();
            for (ClaimedCleanupRecord& claimed : claimedRecords) {
                if (!claimed.removed) continue;
                NotifyIfObserved([&](ManagerObservable& observable) {
                    observable.ThreadRemoved(claimed.snapshot);
                });
                delete claimed.thread;
                claimed.thread = nullptr;
                ++result.ThreadsDeleted;
            }

            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.CleanupCompleted(result);
            });
            return result;
        } catch (...) {
            if (iterationLock.owns_lock()) iterationLock.unlock();
            auto cause = std::current_exception();
            NotifyIfObserved([&](ManagerObservable& observable) {
                observable.CleanupFailed(result, cause);
            });
            throw;
        }
    }

    /// <summary>Runs automatic cleanup and discards the detailed result.</summary>
    void CleanUp() { static_cast<void>(CleanUpWithResult()); }

    /// <summary>Initializes a snapshot of all registered threads.</summary>
    /// <returns>One initialization result for each thread examined in externally preferred owning storage.</returns>
    InitializationResultStorage InitializeWithResults() {
        IterationGuard iteration(*this);
        InitializationTargetStorage snapshot;
        InitializationResultStorage results;

        _threads.WithSharedReadLock([&](const auto& threads) {
            snapshot.reserve(threads.size());
            for (const ThreadRecord& record : threads) snapshot.push_back({record.thread, record.id});
        });
        results.reserve(snapshot.size());

        for (const ThreadInitializationTarget& target : snapshot) {
            ThreadInitializationStatus status = ThreadInitializationStatus::InitializationException;
            try { status = target.thread->Initialize(); } catch (...) {}
            results.push_back({target.id, status});
        }

        ThreadManagerInitializationResult summary;
        summary.ThreadsExamined = results.size();
        for (const ThreadInitializationResult& initialization : results) {
            if (initialization.status == ThreadInitializationStatus::Success) {
                ++summary.ThreadsInitializedSuccessfully;
            } else {
                ++summary.ThreadsInitializationFailed;
            }
        }
        NotifyIfObserved([&](ManagerObservable& observable) {
            observable.InitializationCompleted(summary);
        });
        return results;
    }

    /// <summary>Initializes all registered threads and discards individual results.</summary>
    void Initialize() { static_cast<void>(InitializeWithResults()); }

    /// <summary>Registers an observer for manager-level lifecycle notifications, materializing external-preferred observer bookkeeping on first use.</summary>
    Observable::ObserverHandlePtr RegisterObserver(IThreadManagerObserver* observer) {
        if (observer == nullptr) return {};
        auto observable = EnsureObservable();
        return observable ? observable->RegisterObserver(observer) : Observable::ObserverHandlePtr{};
    }

    /// <summary>Unregisters a manager observer without materializing observer infrastructure when none exists.</summary>
    void UnregisterObserver(IThreadManagerObserver* observer) {
        auto observable = ObservableSnapshot();
        if (observable) observable->UnregisterObserver(observer);
    }

    /// <summary>Returns the number of currently registered threads.</summary>
    std::size_t GetThreadCount() {
        std::size_t result = 0;
        _threads.WithSharedReadLock([&](const auto& threads) { result = threads.size(); });
        return result;
    }
};

} // namespace Threads
} // namespace ESPressio