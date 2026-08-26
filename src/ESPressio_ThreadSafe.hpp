#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

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
#include <mutex>
#include <shared_mutex>
#include <utility>

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

                virtual void WithReadLock(
                    const MutableCallback& callback
                ) = 0;

                virtual void WithWriteLock(
                    const MutableCallback& callback
                ) = 0;

                virtual bool TryWithReadLock(
                    const MutableCallback& callback
                ) = 0;

                virtual bool TryWithWriteLock(
                    const MutableCallback& callback
                ) = 0;

                virtual void WithSharedReadLock(
                    const SharedReadCallback& callback
                ) {
                    WithReadLock([&](T& value) {
                        callback(value);
                    });
                }

                virtual bool TryWithSharedReadLock(
                    const SharedReadCallback& callback
                ) {
                    return TryWithReadLock([&](T& value) {
                        callback(value);
                    });
                }

                virtual void ReleaseLock() = 0;

                virtual void ReleaseReadLock() {
                    ReleaseLock();
                }

                virtual void ReleaseWriteLock() {
                    ReleaseLock();
                }
        };


        template <typename T>
        class Mutex : public IThreadSafe<T> {
            private:
                using MutableCallback = typename IThreadSafe<T>::MutableCallback;
                using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;

                T _value;
                std::mutex _mutex;

                std::function<void(T,T)> _onChange = nullptr;

                std::function<bool(T,T)> _onCompare =
                    [](const T& a, const T& b) -> bool {
                        return a == b;
                    };

            public:
                Mutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) :
                    _value(std::move(value)),
                    _onChange(std::move(onChange)) {

                    if (onCompare != nullptr) {
                        _onCompare = std::move(onCompare);
                    }
                }

                ~Mutex() override = default;

                T Get() override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _value;
                }

                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::unique_lock<std::mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return std::make_pair(false, std::move(defaultValue));
                    }

                    return std::make_pair(true, _value);
                }

                std::function<void(T,T)> GetOnChange() {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _onChange;
                }

                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;

                    {
                        std::lock_guard<std::mutex> lock(_mutex);

                        oldValue = _value;

                        if (_onCompare(oldValue, value)) {
                            return;
                        }

                        _value = std::move(value);
                        onChange = _onChange;
                    }

                    if (onChange != nullptr) {
                        onChange(oldValue, _value);
                    }
                }

                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;

                    {
                        std::unique_lock<std::mutex> lock(
                            _mutex,
                            std::try_to_lock
                        );

                        if (!lock.owns_lock()) {
                            return false;
                        }

                        oldValue = _value;

                        if (_onCompare(oldValue, value)) {
                            return true;
                        }

                        _value = std::move(value);
                        onChange = _onChange;
                    }

                    if (onChange != nullptr) {
                        onChange(oldValue, _value);
                    }

                    return true;
                }

                bool IsLockedRead() override {
                    if (!_mutex.try_lock()) {
                        return true;
                    }
                    _mutex.unlock();
                    return false;
                }

                bool IsLockedWrite() override {
                    return IsLockedRead();
                }

                void WithReadLock(
                    const MutableCallback& callback
                ) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                void WithWriteLock(
                    const MutableCallback& callback
                ) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithReadLock(
                    const MutableCallback& callback
                ) override {
                    std::unique_lock<std::mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return false;
                    }

                    callback(_value);
                    return true;
                }

                bool TryWithWriteLock(
                    const MutableCallback& callback
                ) override {
                    return TryWithReadLock(callback);
                }

                void WithSharedReadLock(
                    const SharedReadCallback& callback
                ) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithSharedReadLock(
                    const SharedReadCallback& callback
                ) override {
                    std::unique_lock<std::mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return false;
                    }

                    callback(_value);
                    return true;
                }

                void ReleaseLock() override {
                    ReleaseReadLock();
                }

                void ReleaseReadLock() override {
                    _mutex.unlock();
                }

                void ReleaseWriteLock() override {
                    _mutex.unlock();
                }
        };


        template <typename T>
        class ReadWriteMutex : public IThreadSafe<T> {
            private:
                using MutableCallback = typename IThreadSafe<T>::MutableCallback;
                using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;

                T _value;
                std::shared_mutex _mutex;

                std::function<void(T,T)> _onChange = nullptr;

                std::function<bool(T,T)> _onCompare =
                    [](const T& a, const T& b) -> bool {
                        return a == b;
                    };

            public:
                ReadWriteMutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) :
                    _value(std::move(value)),
                    _onChange(std::move(onChange)) {

                    if (onCompare != nullptr) {
                        _onCompare = std::move(onCompare);
                    }
                }

                ~ReadWriteMutex() override = default;

                T Get() override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _value;
                }

                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::shared_lock<std::shared_mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return std::make_pair(false, std::move(defaultValue));
                    }

                    return std::make_pair(true, _value);
                }

                std::function<void(T,T)> GetOnChange() {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _onChange;
                }

                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;

                    {
                        std::unique_lock<std::shared_mutex> lock(_mutex);

                        oldValue = _value;

                        if (_onCompare(oldValue, value)) {
                            return;
                        }

                        _value = std::move(value);
                        onChange = _onChange;
                    }

                    if (onChange != nullptr) {
                        onChange(oldValue, _value);
                    }
                }

                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;

                    {
                        std::unique_lock<std::shared_mutex> lock(
                            _mutex,
                            std::try_to_lock
                        );

                        if (!lock.owns_lock()) {
                            return false;
                        }

                        oldValue = _value;

                        if (_onCompare(oldValue, value)) {
                            return true;
                        }

                        _value = std::move(value);
                        onChange = _onChange;
                    }

                    if (onChange != nullptr) {
                        onChange(oldValue, _value);
                    }

                    return true;
                }

                bool IsLockedRead() override {
                    if (!_mutex.try_lock_shared()) {
                        return true;
                    }
                    _mutex.unlock_shared();
                    return false;
                }

                bool IsLockedWrite() override {
                    if (!_mutex.try_lock()) {
                        return true;
                    }
                    _mutex.unlock();
                    return false;
                }

                void WithReadLock(
                    const MutableCallback& callback
                ) override {
                    // A mutable reference requires exclusive ownership.
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithReadLock(
                    const MutableCallback& callback
                ) override {
                    std::unique_lock<std::shared_mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return false;
                    }

                    callback(_value);
                    return true;
                }

                void WithSharedReadLock(
                    const SharedReadCallback& callback
                ) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithSharedReadLock(
                    const SharedReadCallback& callback
                ) override {
                    std::shared_lock<std::shared_mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return false;
                    }

                    callback(_value);
                    return true;
                }

                void ReleaseLock() override {
                    ReleaseReadLock();
                }

                void ReleaseReadLock() override {
                    _mutex.unlock_shared();
                }

                void WithWriteLock(
                    const MutableCallback& callback
                ) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithWriteLock(
                    const MutableCallback& callback
                ) override {
                    std::unique_lock<std::shared_mutex> lock(
                        _mutex,
                        std::try_to_lock
                    );

                    if (!lock.owns_lock()) {
                        return false;
                    }

                    callback(_value);
                    return true;
                }

                void ReleaseWriteLock() override {
                    _mutex.unlock();
                }
        };

    }

}
