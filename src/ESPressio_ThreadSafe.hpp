#pragma once

#if defined(max)
    #pragma push_macro("max")
    #undef max
    #define ESPRESSIO_THREADS_RESTORE_MAX_MACRO
#endif
#if defined(min)
    #pragma push_macro("min")
    #undef min
    #define ESPRESSIO_THREADS_RESTORE_MIN_MACRO
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Synchronization.hpp>

#if defined(ESPRESSIO_THREADS_RESTORE_MIN_MACRO)
    #pragma pop_macro("min")
    #undef ESPRESSIO_THREADS_RESTORE_MIN_MACRO
#endif
#if defined(ESPRESSIO_THREADS_RESTORE_MAX_MACRO)
    #pragma pop_macro("max")
    #undef ESPRESSIO_THREADS_RESTORE_MAX_MACRO
#endif

namespace ESPressio {
namespace Threads {

/// <summary>Common synchronized-value contract supporting copy access, try operations, and callback-based read/write locking.</summary>
template <typename T>
class IThreadSafe {
public:
    using MutableCallback = std::function<void(T&)>;
    using SharedReadCallback = std::function<void(const T&)>;
    virtual ~IThreadSafe() = default;
    virtual T Get() = 0;
    virtual std::pair<bool, T> TryGet(T defaultValue) = 0;
    virtual void Set(T value) = 0;
    virtual bool TrySet(T value) = 0;
    virtual bool IsLockedRead() = 0;
    virtual bool IsLockedWrite() = 0;
    virtual void WithReadLock(const MutableCallback& callback) = 0;
    virtual void WithWriteLock(const MutableCallback& callback) = 0;
    virtual bool TryWithReadLock(const MutableCallback& callback) = 0;
    virtual bool TryWithWriteLock(const MutableCallback& callback) = 0;
    virtual void WithSharedReadLock(const SharedReadCallback& callback) {
        WithReadLock([&](T& value) { callback(value); });
    }
    virtual bool TryWithSharedReadLock(const SharedReadCallback& callback) {
        return TryWithReadLock([&](T& value) { callback(value); });
    }
    virtual void ReleaseLock() = 0;
    virtual void ReleaseReadLock() { ReleaseLock(); }
    virtual void ReleaseWriteLock() { ReleaseLock(); }
};

/// <summary>Optional comparison and change callbacks used by synchronized-value implementations.</summary>
template <typename T>
class ThreadSafeCallbacks {
public:
    std::function<void(T,T)> OnChange = nullptr;
    std::function<bool(T,T)> OnCompare = nullptr;

    ThreadSafeCallbacks(
        std::function<void(T,T)> onChange,
        std::function<bool(T,T)> onCompare
    ) : OnChange(std::move(onChange)), OnCompare(std::move(onCompare)) {}
};

/// <summary>System-mutex synchronized value wrapper.</summary>
template <typename T>
class Mutex : public IThreadSafe<T> {
private:
    using MutableCallback = typename IThreadSafe<T>::MutableCallback;
    using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;
    static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;
    using CallbackStorage = System::Memory::UniquePtr<ThreadSafeCallbacks<T>, ExternalPreferred>;

    T _value;
    System::Synchronization::Mutex _mutex;
    CallbackStorage _callbacks;

    bool _equals(const T& a, const T& b) const {
        return _callbacks && _callbacks->OnCompare
            ? _callbacks->OnCompare(a, b)
            : a == b;
    }

    std::function<void(T,T)> _onChange() const {
        return _callbacks ? _callbacks->OnChange : nullptr;
    }

public:
    /// <summary>Creates an exclusive synchronized value with optional change and comparison callbacks.</summary>
    Mutex(
        T value,
        std::function<void(T,T)> onChange = nullptr,
        std::function<bool(T,T)> onCompare = nullptr
    ) : _value(std::move(value)) {
        if (onChange != nullptr || onCompare != nullptr) {
            _callbacks = System::Memory::MakeUnique<
                ThreadSafeCallbacks<T>, ExternalPreferred
            >(std::move(onChange), std::move(onCompare));
        }
    }

    ~Mutex() override = default;

    T Get() override {
        std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
        return _value;
    }

    std::pair<bool, T> TryGet(T defaultValue) override {
        std::unique_lock<System::Synchronization::Mutex> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return {false, std::move(defaultValue)};
        return {true, _value};
    }

    std::function<void(T,T)> GetOnChange() {
        std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
        return _onChange();
    }

    void Set(T value) override {
        T oldValue = value;
        std::function<void(T,T)> onChange;
        {
            std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
            oldValue = _value;
            if (_equals(oldValue, value)) return;
            onChange = _onChange();
            if (onChange == nullptr) {
                _value = std::move(value);
                return;
            }
            _value = value;
        }
        onChange(oldValue, value);
    }

    bool TrySet(T value) override {
        T oldValue = value;
        std::function<void(T,T)> onChange;
        {
            std::unique_lock<System::Synchronization::Mutex> lock(_mutex, std::try_to_lock);
            if (!lock.owns_lock()) return false;
            oldValue = _value;
            if (_equals(oldValue, value)) return true;
            onChange = _onChange();
            if (onChange == nullptr) {
                _value = std::move(value);
                return true;
            }
            _value = value;
        }
        onChange(oldValue, value);
        return true;
    }

    bool IsLockedRead() override {
        if (!_mutex.try_lock()) return true;
        _mutex.unlock();
        return false;
    }

    bool IsLockedWrite() override { return IsLockedRead(); }

    void WithReadLock(const MutableCallback& callback) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
        callback(_value);
    }

    void WithWriteLock(const MutableCallback& callback) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
        callback(_value);
    }

    bool TryWithReadLock(const MutableCallback& callback) override {
        std::unique_lock<System::Synchronization::Mutex> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        callback(_value);
        return true;
    }

    bool TryWithWriteLock(const MutableCallback& callback) override {
        return TryWithReadLock(callback);
    }

    void WithSharedReadLock(const SharedReadCallback& callback) override {
        std::lock_guard<System::Synchronization::Mutex> lock(_mutex);
        callback(_value);
    }

    bool TryWithSharedReadLock(const SharedReadCallback& callback) override {
        std::unique_lock<System::Synchronization::Mutex> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        callback(_value);
        return true;
    }

    void ReleaseLock() override { _mutex.unlock(); }
    void ReleaseReadLock() override { _mutex.unlock(); }
    void ReleaseWriteLock() override { _mutex.unlock(); }
};

/// <summary>System read/write-lock synchronized value wrapper.</summary>
/// <remarks>The active platform provider controls whether shared readers execute concurrently. ESP32 currently serializes shared and exclusive acquisition through one native FreeRTOS mutex to avoid pthread rwlock state.</remarks>
template <typename T>
class ReadWriteMutex : public IThreadSafe<T> {
private:
    using MutableCallback = typename IThreadSafe<T>::MutableCallback;
    using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;
    static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;
    using CallbackStorage = System::Memory::UniquePtr<ThreadSafeCallbacks<T>, ExternalPreferred>;

    T _value;
    System::Synchronization::ReadWriteLock _mutex;
    CallbackStorage _callbacks;

    bool _equals(const T& a, const T& b) const {
        return _callbacks && _callbacks->OnCompare
            ? _callbacks->OnCompare(a, b)
            : a == b;
    }

    std::function<void(T,T)> _onChange() const {
        return _callbacks ? _callbacks->OnChange : nullptr;
    }

public:
    /// <summary>Creates a read/write synchronized value with optional change and comparison callbacks.</summary>
    ReadWriteMutex(
        T value,
        std::function<void(T,T)> onChange = nullptr,
        std::function<bool(T,T)> onCompare = nullptr
    ) : _value(std::move(value)) {
        if (onChange != nullptr || onCompare != nullptr) {
            _callbacks = System::Memory::MakeUnique<
                ThreadSafeCallbacks<T>, ExternalPreferred
            >(std::move(onChange), std::move(onCompare));
        }
    }

    ~ReadWriteMutex() override = default;

    T Get() override {
        std::shared_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
        return _value;
    }

    std::pair<bool, T> TryGet(T defaultValue) override {
        std::shared_lock<System::Synchronization::ReadWriteLock> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return {false, std::move(defaultValue)};
        return {true, _value};
    }

    std::function<void(T,T)> GetOnChange() {
        std::shared_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
        return _onChange();
    }

    void Set(T value) override {
        T oldValue = value;
        std::function<void(T,T)> onChange;
        {
            std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
            oldValue = _value;
            if (_equals(oldValue, value)) return;
            onChange = _onChange();
            if (onChange == nullptr) {
                _value = std::move(value);
                return;
            }
            _value = value;
        }
        onChange(oldValue, value);
    }

    bool TrySet(T value) override {
        T oldValue = value;
        std::function<void(T,T)> onChange;
        {
            std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex, std::try_to_lock);
            if (!lock.owns_lock()) return false;
            oldValue = _value;
            if (_equals(oldValue, value)) return true;
            onChange = _onChange();
            if (onChange == nullptr) {
                _value = std::move(value);
                return true;
            }
            _value = value;
        }
        onChange(oldValue, value);
        return true;
    }

    bool IsLockedRead() override {
        if (!_mutex.try_lock_shared()) return true;
        _mutex.unlock_shared();
        return false;
    }

    bool IsLockedWrite() override {
        if (!_mutex.try_lock()) return true;
        _mutex.unlock();
        return false;
    }

    void WithReadLock(const MutableCallback& callback) override {
        std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
        callback(_value);
    }

    bool TryWithReadLock(const MutableCallback& callback) override {
        std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        callback(_value);
        return true;
    }

    void WithSharedReadLock(const SharedReadCallback& callback) override {
        std::shared_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
        callback(_value);
    }

    bool TryWithSharedReadLock(const SharedReadCallback& callback) override {
        std::shared_lock<System::Synchronization::ReadWriteLock> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        callback(_value);
        return true;
    }

    void WithWriteLock(const MutableCallback& callback) override {
        std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex);
        callback(_value);
    }

    bool TryWithWriteLock(const MutableCallback& callback) override {
        std::unique_lock<System::Synchronization::ReadWriteLock> lock(_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        callback(_value);
        return true;
    }

    void ReleaseLock() override { ReleaseReadLock(); }
    void ReleaseReadLock() override { _mutex.unlock_shared(); }
    void ReleaseWriteLock() override { _mutex.unlock(); }
};

} // namespace Threads
} // namespace ESPressio
