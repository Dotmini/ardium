#pragma once

#include <concepts>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 1: UNIVERSAL HAL
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  Defines the abstract interface for the Hardware Abstraction Layer (HAL).
 *  Uses C++20 concepts to enforce contract compliance at compile-time.
 *  Supports dynamic dispatch for platform-specific implementations.
 * ============================================================================
 */

// --- PLATFORM DETECTION MACROS ---
#if defined(__APPLE__) && defined(__MACH__)
    #define ARDIUM_PLATFORM_MAC 1
    #define ARDIUM_HAL_POSIX 1
#elif defined(ESP32) || defined(ARDUINO)
    #define ARDIUM_PLATFORM_ESP32 1
    #define ARDIUM_HAL_FREERTOS 1
#elif defined(__linux__)
    #define ARDIUM_PLATFORM_LINUX 1
    #define ARDIUM_HAL_POSIX 1
#else
    #error "[ArdiumOS] Critical Failure: Unsupported Platform Detected."
#endif

namespace Ardium {
namespace HAL {

    // --- CONCEPTS ---

    /**
     * @brief Concept for a Lockable object (Mutex).
     */
    template<typename T>
    concept Lockable = requires(T a) {
        { a.lock() } -> std::same_as<void>;
        { a.unlock() } -> std::same_as<void>;
        { a.try_lock() } -> std::same_as<bool>;
    };

    /**
     * @brief Concept for a Runnable task.
     */
    template<typename F>
    concept Runnable = std::invocable<F>;

    // --- ABSTRACT INTERFACES ---

    /**
     * @class ISystemClock
     * @brief High-precision timing interface.
     */
    class ISystemClock {
    public:
        virtual ~ISystemClock() = default;
        
        /**
         * @return Current system time in nanoseconds since boot.
         */
        virtual uint64_t now_nanos() const = 0;

        /**
         * @return Current system time in microseconds since boot.
         */
        virtual uint64_t now_micros() const = 0;
    };

    /**
     * @class IMutex
     * @brief Abstract Mutex interface.
     */
    class IMutex {
    public:
        virtual ~IMutex() = default;
        virtual void lock() = 0;
        virtual void unlock() = 0;
        virtual bool try_lock() = 0;
    };

    /**
     * @class IThread
     * @brief Abstract Threading interface.
     * Manages execution context and priority.
     */
    class IThread {
    public:
        enum class Priority {
            Low,
            Normal,
            High,
            RealTime
        };

        virtual ~IThread() = default;

        /**
         * @brief Spawn a new thread execution context.
         * @param func The function to execute.
         * @param priority The OS scheduling priority.
         * @param name Debug name for the thread.
         */
        virtual void spawn(std::function<void()> func, Priority priority, const std::string& name) = 0;

        /**
         * @brief Blocks calling thread until this thread finishes.
         */
        virtual void join() = 0;

        /**
         * @brief Detaches the thread, allowing it to run independently.
         */
        virtual void detach() = 0;
        
        /**
         * @return The native OS handle (pthread_t, TaskHandle_t, etc.) cast to void*.
         */
        virtual void* native_handle() = 0;
    };

    /**
     * @class IFile
     * @brief Abstract File I/O interface.
     */
    class IFile {
    public:
        enum class Mode {
            Read,
            Write,
            Append,
            ReadWrite
        };

        virtual ~IFile() = default;

        virtual bool open(const std::string& path, Mode mode) = 0;
        virtual void close() = 0;
        virtual size_t read(void* buffer, size_t size) = 0;
        virtual size_t write(const void* data, size_t size) = 0;
        virtual uint64_t size() const = 0;
        virtual bool isOpen() const = 0;
    };

    // --- FACTORY (To be implemented by OS specific .mm/.cpp) ---
    
    struct OSFactory {
        static std::unique_ptr<ISystemClock> CreateClock();
        static std::unique_ptr<IMutex> CreateMutex();
        static std::unique_ptr<IThread> CreateThread();
        static std::unique_ptr<IFile> CreateFile();
    };

} // namespace HAL
} // namespace Ardium
