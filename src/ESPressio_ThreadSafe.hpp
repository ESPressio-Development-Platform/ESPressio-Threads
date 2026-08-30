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

#include <ESPressio_Memory.hpp>

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
        /// <typeparam name="T">Protected value type.</typeparam>
        template <typename T>
        class IThreadSafe {
            public:
                /// <summary>Callback receiving mutable access to the protected value while a lock is held.</summary>
                using MutableCallback = std::function<void(T&)>;
                /// <summary>Callback receiving const access to the protected value while a shared/read lock is held.</summary>
                using SharedReadCallback = std::function<void(const T&)>;

                virtual ~IThreadSafe() = default;

                /// <summary>Returns a copy of the protected value after acquiring the required read lock.</summary>
                virtual T Get() = 0;
                /// <summary>Attempts to return a protected-value copy without blocking.</summary>
                /// <returns>A success flag and either the protected value or supplied default.</returns>
                virtual std::pair<bool, T> TryGet(T defaultValue) = 0;
                /// <summary>Replaces the protected value, blocking until the required write lock is acquired.</summary>
                virtual void Set(T value) = 0;
                /// <summary>Attempts to replace the protected value without blocking.</summary>
                virtual bool TrySet(T value) = 0;

                /// <summary>Indicates whether an immediate read lock cannot currently be acquired.</summary>
                virtual bool IsLockedRead() = 0;
                /// <summary>Indicates whether an immediate write lock cannot currently be acquired.</summary>
                virtual bool IsLockedWrite() = 0;

                /// <summary>Invokes a callback while holding the implementation's read lock with mutable compatibility access.</summary>
                virtual void WithReadLock(const MutableCallback& callback) = 0;
                /// <summary>Invokes a callback while holding the implementation's write lock.</summary>
                virtual void WithWriteLock(const MutableCallback& callback) = 0;
                /// <summary>Attempts to invoke a callback under a read lock without blocking.</summary>
                virtual bool TryWithReadLock(const MutableCallback& callback) = 0;
                /// <summary>Attempts to invoke a callback under a write lock without blocking.</summary>
                virtual bool TryWithWriteLock(const MutableCallback& callback) = 0;

                /// <summary>Invokes a callback with const access while holding a read lock.</summary>
                virtual void WithSharedReadLock(
                    const SharedReadCallback& callback
                ) {
                    WithReadLock([&](T& value) { callback(value); });
                }

                /// <summary>Attempts to invoke a callback with const access under a read lock without blocking.</summary>
                virtual bool TryWithSharedReadLock(
                    const SharedReadCallback& callback
                ) {
                    return TryWithReadLock([&](T& value) { callback(value); });
                }

                /// <summary>Releases the currently held implementation lock when manual locking is used.</summary>
                virtual void ReleaseLock() = 0;
                /// <summary>Releases a manually held read lock.</summary>
                virtual void ReleaseReadLock() { ReleaseLock(); }
                /// <summary>Releases a manually held write lock.</summary>
                virtual void ReleaseWriteLock() { ReleaseLock(); }
        };


        /// <summary>Optional comparison and change callbacks used by synchronized-value implementations.</summary>
        template <typename T>
        class ThreadSafeCallbacks {
            public:
                /// <summary>Invoked after the protected value changes.</summary>
                std::function<void(T,T)> OnChange = nullptr;
                /// <summary>Optional equality predicate used instead of <c>operator==</c>.</summary>
                std::function<bool(T,T)> OnCompare = nullptr;

                ThreadSafeCallbacks(
                    std::function<void(T,T)> onChange,
                    std::function<bool(T,T)> onCompare
                ) :
                    OnChange(std::move(onChange)),
                    OnCompare(std::move(onCompare)) {
                }
        };


        /// <summary>Exclusive-mutex synchronized value wrapper.</summary>
        /// <typeparam name="T">Protected value type.</typeparam>
        template <typename T>
        class Mutex : public IThreadSafe<T> {
            private:
                using MutableCallback = typename IThreadSafe<T>::MutableCallback;
                using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;
                static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;
                using CallbackStorage = System::Memory::UniquePtr<ThreadSafeCallbacks<T>, ExternalPreferred>;

                T _value;
                std::mutex _mutex;
                CallbackStorage _callbacks;

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
                /// <summary>Creates an exclusive synchronized value with optional change and comparison callbacks.</summary>
                /// <remarks>Optional callback state is externally preferred because it is ordinary application metadata and does not require internal/DMA-capable memory.</remarks>
                Mutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) : _value(std::move(value)) {
                    if (onChange != nullptr || onCompare != nullptr) {
                        _callbacks = System::Memory::MakeUnique<
                            ThreadSafeCallbacks<T>,
                            ExternalPreferred
                        >(
                            std::move(onChange),
                            std::move(onCompare)
                        );
                    }
                }

                ~Mutex() override = default;

                /// <inheritdoc/>
                T Get() override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _value;
                }

                /// <inheritdoc/>
                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return std::make_pair(false, std::move(defaultValue));
                    return std::make_pair(true, _value);
                }

                /// <summary>Returns the currently configured change callback.</summary>
                std::function<void(T,T)> GetOnChange() {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _onChange();
                }

                /// <inheritdoc/>
                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
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

                /// <inheritdoc/>
                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
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

                /// <inheritdoc/>
                bool IsLockedRead() override {
                    if (!_mutex.try_lock()) return true;
                    _mutex.unlock();
                    return false;
                }

                /// <inheritdoc/>
                bool IsLockedWrite() override { return IsLockedRead(); }

                /// <inheritdoc/>
                void WithReadLock(const MutableCallback& callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                void WithWriteLock(const MutableCallback& callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                bool TryWithReadLock(const MutableCallback& callback) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                /// <inheritdoc/>
                bool TryWithWriteLock(const MutableCallback& callback) override {
                    return TryWithReadLock(callback);
                }

                /// <inheritdoc/>
                void WithSharedReadLock(const SharedReadCallback& callback) override {
                    std::lock_guard<std::mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                bool TryWithSharedReadLock(const SharedReadCallback& callback) override {
                    std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                /// <inheritdoc/>
                void ReleaseLock() override { ReleaseReadLock(); }
                /// <inheritdoc/>
                void ReleaseReadLock() override { _mutex.unlock(); }
                /// <inheritdoc/>
                void ReleaseWriteLock() override { _mutex.unlock(); }
        };


        /// <summary>Shared/read-write mutex synchronized value wrapper supporting concurrent const readers.</summary>
        /// <typeparam name="T">Protected value type.</typeparam>
        template <typename T>
        class ReadWriteMutex : public IThreadSafe<T> {
            private:
                using MutableCallback = typename IThreadSafe<T>::MutableCallback;
                using SharedReadCallback = typename IThreadSafe<T>::SharedReadCallback;
                static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;
                using CallbackStorage = System::Memory::UniquePtr<ThreadSafeCallbacks<T>, ExternalPreferred>;

                T _value;
                std::shared_mutex _mutex;
                CallbackStorage _callbacks;

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
                /// <summary>Creates a read/write synchronized value with optional change and comparison callbacks.</summary>
                /// <remarks>Optional callback state is externally preferred because it is ordinary application metadata and does not require internal/DMA-capable memory.</remarks>
                ReadWriteMutex(
                    T value,
                    std::function<void(T,T)> onChange = nullptr,
                    std::function<bool(T,T)> onCompare = nullptr
                ) : _value(std::move(value)) {
                    if (onChange != nullptr || onCompare != nullptr) {
                        _callbacks = System::Memory::MakeUnique<
                            ThreadSafeCallbacks<T>,
                            ExternalPreferred
                        >(
                            std::move(onChange),
                            std::move(onCompare)
                        );
                    }
                }

                ~ReadWriteMutex() override = default;

                /// <inheritdoc/>
                T Get() override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _value;
                }

                /// <inheritdoc/>
                std::pair<bool, T> TryGet(T defaultValue) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return std::make_pair(false, std::move(defaultValue));
                    return std::make_pair(true, _value);
                }

                /// <summary>Returns the currently configured change callback.</summary>
                std::function<void(T,T)> GetOnChange() {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _onChange();
                }

                /// <inheritdoc/>
                void Set(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::shared_mutex> lock(_mutex);
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

                /// <inheritdoc/>
                bool TrySet(T value) override {
                    T oldValue = value;
                    std::function<void(T,T)> onChange;
                    {
                        std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
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

                /// <inheritdoc/>
                bool IsLockedRead() override {
                    if (!_mutex.try_lock_shared()) return true;
                    _mutex.unlock_shared();
                    return false;
                }

                /// <inheritdoc/>
                bool IsLockedWrite() override {
                    if (!_mutex.try_lock()) return true;
                    _mutex.unlock();
                    return false;
                }

                /// <inheritdoc/>
                void WithReadLock(const MutableCallback& callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                bool TryWithReadLock(const MutableCallback& callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                /// <inheritdoc/>
                void WithSharedReadLock(const SharedReadCallback& callback) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                bool TryWithSharedReadLock(const SharedReadCallback& callback) override {
                    std::shared_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                /// <inheritdoc/>
                void ReleaseLock() override { ReleaseReadLock(); }
                /// <inheritdoc/>
                void ReleaseReadLock() override { _mutex.unlock_shared(); }

                /// <inheritdoc/>
                void WithWriteLock(const MutableCallback& callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                /// <inheritdoc/>
                bool TryWithWriteLock(const MutableCallback& callback) override {
                    std::unique_lock<std::shared_mutex> lock(_mutex, std::try_to_lock);
                    if (!lock.owns_lock()) return false;
                    callback(_value);
                    return true;
                }

                /// <inheritdoc/>
                void ReleaseWriteLock() override { _mutex.unlock(); }
        };

    }

}
