#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <ESPressio_Memory.hpp>
#include "ESPressio_ThreadSafe.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"
#include "ESPressio_IThreadManagerObserver.hpp"
#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

struct ThreadInitializationResult {
    uint8_t threadID;
    ThreadInitializationStatus status;
};

class ThreadManager {
private:
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
        System::Memory::MemoryPolicy::ExternalPreferred
    >;
    using ThreadPointerSnapshot = System::Memory::Vector<
        IThread*,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;
    using CleanupRecordStorage = System::Memory::Vector<
        ClaimedCleanupRecord,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;
    using InitializationTargetStorage = System::Memory::Vector<
        ThreadInitializationTarget,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    ReadWriteMutex<ThreadRecordStorage> _threads;
    ReadWriteMutex<int> _nextCoreID = ReadWriteMutex<int>(0);
    std::recursive_mutex _iterationMutex;
    std::size_t _activeIterations = 0;
    bool _cleanupPending = false;
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
#if defined(portNUM_PROCESSORS)
        return portNUM_PROCESSORS > 0 ? portNUM_PROCESSORS : 1;
#elif defined(configNUMBER_OF_CORES)
        return configNUMBER_OF_CORES > 0 ? configNUMBER_OF_CORES : 1;
#else
        return 1;
#endif
    }

protected:
    ThreadManager()
        : _threads(ThreadRecordStorage{}),
          _observable(System::Memory::MakeShared<
              ManagerObservable,
              System::Memory::MemoryPolicy::ExternalPreferred
          >()) {}

public:
    static ThreadManager* GetInstance() {
        // Manager/control state remains in normal/internal memory; only its
        // variable-size registry and snapshots use ExternalPreferred storage.
        static ThreadManager* instance = new ThreadManager();
        return instance;
    }

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
            if (inserted) _observable->ThreadRegistered(thread, _snapshot(resolved));
            return resolved.coreID;
        } catch (...) {
            _observable->ThreadRegistrationFailed(thread, std::current_exception());
            throw;
        }
    }

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
        if (removed) _observable->ThreadRemoved(_snapshot(removedRecord));
    }

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
        if (removed) _observable->ThreadRemoved(_snapshot(removedRecord));
    }

    void ForEachThread(const std::function<void(IThread*)>& callback) {
        IterationGuard iteration(*this);
        ThreadPointerSnapshot snapshot;
        _threads.WithSharedReadLock([&](const auto& threads) {
            snapshot.reserve(threads.size());
            for (const ThreadRecord& record : threads) snapshot.push_back(record.thread);
        });
        for (IThread* thread : snapshot) callback(thread);
    }

    bool WithThread(uint8_t threadID, const std::function<void(IThread*)>& callback) {
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

    IThread* GetThread(uint8_t threadID) {
        IThread* result = nullptr;
        _threads.WithSharedReadLock([&](const auto& threads) {
            for (const ThreadRecord& record : threads) {
                if (record.id == threadID) { result = record.thread; break; }
            }
        });
        return result;
    }

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
            _observable->CleanupDeferred(result);
            return result;
        }

        try {
            _threads.WithSharedReadLock([&](const auto& threads) {
                snapshot.reserve(threads.size());
                snapshot.insert(snapshot.end(), threads.begin(), threads.end());
            });
            result.ThreadsExamined = snapshot.size();
            result.ThreadCountBefore = snapshot.size();
            _observable->CleanupStarted(result);

            claimedRecords.reserve(snapshot.size());
            for (const ThreadRecord& record : snapshot) {
                if (record.thread != nullptr &&
                    record.thread->GetThreadState() == ThreadState::Terminated &&
                    record.thread->TryClaimAutomaticCleanup()) {
                    claimedRecords.push_back({record.id, record.thread, _snapshot(record), false});
                    ++result.ThreadsClaimed;
                    _observable->CleanupClaimed(record.thread, claimedRecords.back().snapshot);
                }
            }

            // The full record snapshot is no longer needed after claims have
            // been resolved; release its external storage before deletion work.
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
                _observable->ThreadRemoved(claimed.snapshot);
                delete claimed.thread;
                claimed.thread = nullptr;
                ++result.ThreadsDeleted;
            }

            _observable->CleanupCompleted(result);
            return result;
        } catch (...) {
            if (iterationLock.owns_lock()) iterationLock.unlock();
            _observable->CleanupFailed(result, std::current_exception());
            throw;
        }
    }

    void CleanUp() { static_cast<void>(CleanUpWithResult()); }

    std::vector<ThreadInitializationResult> InitializeWithResults() {
        IterationGuard iteration(*this);
        InitializationTargetStorage snapshot;
        std::vector<ThreadInitializationResult> results;

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
        _observable->InitializationCompleted(summary);
        return results;
    }

    void Initialize() { static_cast<void>(InitializeWithResults()); }

    Observable::ObserverHandlePtr RegisterObserver(IThreadManagerObserver* observer) {
        return _observable->RegisterObserver(observer);
    }

    void UnregisterObserver(IThreadManagerObserver* observer) {
        _observable->UnregisterObserver(observer);
    }

    std::size_t GetThreadCount() {
        std::size_t result = 0;
        _threads.WithSharedReadLock([&](const auto& threads) { result = threads.size(); });
        return result;
    }
};

} // namespace Threads
} // namespace ESPressio
