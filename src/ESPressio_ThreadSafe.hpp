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
#include <memory>
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
                virtual ~IThreadSafe() = default;

                virtual T Get() = 0;
                virtual std::pair<bool, T> TryGet(T defaultValue) = 0;
                virtual void Set(T value) = 0;
                virtual bool TrySet(T value) = 0;

                virtual bool IsLockedRead() = 0;
                virtual bool IsLockedWrite() = 0;

                virtual void WithReadLock(std::function<void(T&)> callback) = 0;
                virtual void WithWriteLock(std::function<void(T&)> callback) = 0;
                virtual bool TryWithReadLock(std::function<void(T&)> callback) = 0;
                virtual bool TryWithWriteLock(std::function<void(T&)> callback) = 0;

                virtual void WithSharedReadLock(
                    std::function<void(const T&)> callback
                ) {
                    WithReadLock([&](T& value) { callback(value); });
                }

                virtual bool TryWithSharedReadLock(
                    std::function<void(const T&)> callback
                ) {
                    return TryWithReadLock([&](T& value) { callback(value); });
                }

                virtual void ReleaseLock() = 0;
                virtual void ReleaseReadLock() { ReleaseLock(); }
                virtual void ReleaseWriteLock() { ReleaseLock(); }
        };


        template <typename T>
        class ThreadSafeCallbacks {
            public:
                std::function<void(T,T)> OnChange = nullptr;
                std::function<bool(T,T)> OnCompare = nullptr;

                ThreadSafeCallbacks(
                    std::function<void(T,T)> onChange,
                    std::function<bool(T,T)> onCompare
                ) :
                    OnChange(std::move(onChange)),
                    OnCompare(std::move(onCompare)) {
                }
        };


        template <typename T>
        class Mutex : public IThreadSafe<T> {
            private:
                T _value;
                std::mutex _mutex;
                std::unique_ptr<ThreadSafeCallbacks<T>> _callbacks;

                bool _equals(const T& a, const T& b) const {
                    return
                        _callbacks && _callbacks->OnCompare
                            ? _callbacks->OnCompare(a, b)
                            : a == b;
                }

                std::function<void(T,T)> _onChange() const {
                    return _callbacks ? _callbacks->OnChange : nullptr;
                }

            public:
                Mutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) : _value(value) {
                    if (onChange != nullptr || onCompare != nullptr) {
                        _callbacks = std::make_unique<ThreadSafeCallbacks<T>>(
                            std::move(onChange),
                            std::move(onCompare)
                        );
                    }
                }

                ~Mutex() override = default;

                T Get() override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _value;
                }

                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return std::make_pair(false, defaultValue);
                    return std::make_pair(true, _value);
                }

                std::function<void(T,T)> GetOnChange() {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _onChange();
                }

                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        oldValue = _value;
                        if (_equals(oldValue, value)) return;
                        _value = value;
                        onChange = _onChange();
                    }
                    if (onChange != nullptr) onChange(oldValue, value);
                }

                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                        if (!lock.owns_lock()) return false;
                        oldValue = _value;
                        if (_equals(oldValue, value)) return true;
                        _value = value;
                        onChange = _onChange();
                    }
                    if (onChange != nullptr) onChange(oldValue, value);
                    return true;
                }

                bool IsLockedRead() override {
                    if (_mutex.try_lock()) {
                        _mutex.unlock();
                        return false;
                    }
                    return true;
                }

                bool IsLockedWrite() override { return IsLockedRead(); }

                void WithReadLock(std::function<void(T&)> callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                void WithWriteLock(std::function<void(T&)> callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithReadLock(std::function<void(T&)> callback) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                bool TryWithWriteLock(std::function<void(T&)> callback) override {
                    return TryWithReadLock(std::move(callback));
                }

                void WithSharedReadLock(std::function<void(const T&)> callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithSharedReadLock(std::function<void(const T&)> callback) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                void ReleaseLock() override { ReleaseReadLock(); }
                void ReleaseReadLock() override { _mutex.unlock(); }
                void ReleaseWriteLock() override { _mutex.unlock(); }
        };


        template <typename T>
        class ReadWriteMutex : public IThreadSafe<T> {
            private:
                T _value;
                std::shared_mutex _mutex;
                std::unique_ptr<ThreadSafeCallbacks<T>> _callbacks;

                bool _equals(const T& a, const T& b) const {
                    return
                        _callbacks && _callbacks->OnCompare
                            ? _callbacks->OnCompare(a, b)
                            : a == b;
                }

                std::function<void(T,T)> _onChange() const {
                    return _callbacks ? _callbacks->OnChange : nullptr;
                }

            public:
                ReadWriteMutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) : _value(value) {
                    if (onChange != nullptr || onCompare != nullptr) {
                        _callbacks = std::make_unique<ThreadSafeCallbacks<T>>(
                            std::move(onChange),
                            std::move(onCompare)
                        );
                    }
                }

                ~ReadWriteMutex() override = default;

                T Get() override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _value;
                }

                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return std::make_pair(false, defaultValue);
                    return std::make_pair(true, _value);
                }

                std::function<void(T,T)> GetOnChange() {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _onChange();
                }

                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::shared_mutex> lock(_mutex);
                        oldValue = _value;
                        if (_equals(oldValue, value)) return;
                        _value = value;
                        onChange = _onChange();
                    }
                    if (onChange != nullptr) onChange(oldValue, value);
                }

                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                        if (!lock.owns_lock()) return false;
                        oldValue = _value;
                        if (_equals(oldValue, value)) return true;
                        _value = value;
                        onChange = _onChange();
                    }
                    if (onChange != nullptr) onChange(oldValue, value);
                    return true;
                }

                bool IsLockedRead() override {
                    if (_mutex.try_lock_shared()) {
                        _mutex.unlock_shared();
                        return false;
                    }
                    return true;
                }

                bool IsLockedWrite() override {
                    if (_mutex.try_lock()) {
                        _mutex.unlock();
                        return false;
                    }
                    return true;
                }

                void WithReadLock(std::function<void(T&)> callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithReadLock(std::function<void(T&)> callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                void WithSharedReadLock(std::function<void(const T&)> callback) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithSharedReadLock(std::function<void(const T&)> callback) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                void ReleaseLock() override { ReleaseReadLock(); }
                void ReleaseReadLock() override { _mutex.unlock_shared(); }

                void WithWriteLock(std::function<void(T&)> callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                bool TryWithWriteLock(std::function<void(T&)> callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                void ReleaseWriteLock() override { _mutex.unlock(); }
        };

    }

}
