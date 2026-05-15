#include "../include/ArdiumOS.h"
#import <Foundation/Foundation.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <mutex>
#include <thread>

// --- MEMORY CONTROLLER EXTERNS ---
// We link loosely to the MemoryController for logging to avoid cyclic header deps if not careful.
namespace Ardium::Memory {
    extern void Log(const char* fmt, ...);
}

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 1: MACOS HAL
 * ============================================================================
 *  Architect: Major
 *  Platform:  macOS (Darwin/Mach)
 *  
 *  Description:
 *  Implements ArdiumOS.h using low-level Darwin APIs (Mach, POSIX, pthread).
 *  Ensures high-precision timing and QoS-aware threading.
 * ============================================================================
 */

namespace Ardium::HAL {

    // --- CLOCK IMPLEMENTATION (Mach) ---
    class MacSystemClock : public ISystemClock {
        mach_timebase_info_data_t timebase;
    public:
        MacSystemClock() {
            if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
                Ardium::Memory::Log("[HAL::Clock] Critical Error: Failed to get mach timebase.");
                // Fallback to safe default
                timebase.numer = 1;
                timebase.denom = 1;
            }
        }

        uint64_t now_nanos() const override {
            uint64_t mach_time = mach_absolute_time();
            return mach_time * timebase.numer / timebase.denom;
        }

        uint64_t now_micros() const override {
            return now_nanos() / 1000;
        }
    };

    // --- MUTEX IMPLEMENTATION (std::mutex wrapping pthread) ---
    class MacMutex : public IMutex {
        std::mutex m_mtx;
    public:
        void lock() override {
            // Ardium::Memory::Log("[HAL::Mutex] Locking...");
            m_mtx.lock();
        }

        void unlock() override {
            m_mtx.unlock();
            // Ardium::Memory::Log("[HAL::Mutex] Unlocked.");
        }

        bool try_lock() override {
            return m_mtx.try_lock();
        }
    };

    // --- THREAD IMPLEMENTATION (pthread with QoS) ---
    class MacThread : public IThread {
        std::thread m_thread;
        bool m_joined_or_detached = false;

    public:
        ~MacThread() {
            if (m_thread.joinable()) {
                Ardium::Memory::Log("[HAL::Thread] Destructor called on joinable thread. Terminating.");
                std::terminate();
            }
        }

        void spawn(std::function<void()> func, Priority priority, const std::string& name) override {
            Ardium::Memory::Log("[HAL::Thread] Spawning thread: %s", name.c_str());
            
            m_thread = std::thread([func, priority, name]() {
                // Set Thread Name
                pthread_setname_np(name.c_str());

                // Set QoS Class (Apple Specific)
                qos_class_t qos_class = QOS_CLASS_DEFAULT;
                switch (priority) {
                    case Priority::Low:      qos_class = QOS_CLASS_BACKGROUND; break;
                    case Priority::Normal:   qos_class = QOS_CLASS_USER_INITIATED; break;
                    case Priority::High:     qos_class = QOS_CLASS_USER_INTERACTIVE; break;
                    case Priority::RealTime: qos_class = QOS_CLASS_USER_INTERACTIVE; break; // RealTime needs specific policy, UI Interactive is close enough for general HAL
                }
                
                pthread_set_qos_class_self_np(qos_class, 0);
                
                Ardium::Memory::Log("[HAL::Thread] Thread '%s' started with QoS %d", name.c_str(), qos_class);
                
                try {
                    func();
                } catch (const std::exception& e) {
                    Ardium::Memory::Log("[HAL::Thread] Exception in thread '%s': %s", name.c_str(), e.what());
                } catch (...) {
                    Ardium::Memory::Log("[HAL::Thread] Unknown exception in thread '%s'", name.c_str());
                }
                
                Ardium::Memory::Log("[HAL::Thread] Thread '%s' finished.", name.c_str());
            });
        }

        void join() override {
            if (m_thread.joinable()) {
                m_thread.join();
                m_joined_or_detached = true;
            }
        }

        void detach() override {
            if (m_thread.joinable()) {
                m_thread.detach();
                m_joined_or_detached = true;
            }
        }

        void* native_handle() override {
            return (void*)m_thread.native_handle();
        }
    };

    // --- FILE IMPLEMENTATION (POSIX) ---
    class MacFile : public IFile {
        int m_fd = -1;
    public:
        ~MacFile() {
            if (isOpen()) close();
        }

        bool open(const std::string& path, Mode mode) override {
            int flags = 0;
            switch(mode) {
                case Mode::Read:      flags = O_RDONLY; break;
                case Mode::Write:     flags = O_WRONLY | O_CREAT | O_TRUNC; break;
                case Mode::Append:    flags = O_WRONLY | O_CREAT | O_APPEND; break;
                case Mode::ReadWrite: flags = O_RDWR | O_CREAT; break;
            }

            // Permissions: RW for user, R for group/others
            m_fd = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            
            if (m_fd < 0) {
                Ardium::Memory::Log("[HAL::File] Failed to open file: %s (Errno: %d)", path.c_str(), errno);
                return false;
            }
            return true;
        }

        void close() override {
            if (m_fd >= 0) {
                ::close(m_fd);
                m_fd = -1;
            }
        }

        size_t read(void* buffer, size_t size) override {
            if (m_fd < 0) return 0;
            ssize_t bytes = ::read(m_fd, buffer, size);
            if (bytes < 0) {
                Ardium::Memory::Log("[HAL::File] Read error. Errno: %d", errno);
                return 0;
            }
            return (size_t)bytes;
        }

        size_t write(const void* data, size_t size) override {
            if (m_fd < 0) return 0;
            ssize_t bytes = ::write(m_fd, data, size);
            if (bytes < 0) {
                Ardium::Memory::Log("[HAL::File] Write error. Errno: %d", errno);
                return 0;
            }
            return (size_t)bytes;
        }

        uint64_t size() const override {
            if (m_fd < 0) return 0;
            struct stat st;
            if (fstat(m_fd, &st) == 0) {
                return (uint64_t)st.st_size;
            }
            return 0;
        }

        bool isOpen() const override {
            return m_fd >= 0;
        }
    };

    // --- FACTORY IMPLEMENTATION ---
    std::unique_ptr<ISystemClock> OSFactory::CreateClock() { return std::make_unique<MacSystemClock>(); }
    std::unique_ptr<IMutex> OSFactory::CreateMutex() { return std::make_unique<MacMutex>(); }
    std::unique_ptr<IThread> OSFactory::CreateThread() { return std::make_unique<MacThread>(); }
    std::unique_ptr<IFile> OSFactory::CreateFile() { return std::make_unique<MacFile>(); }

} // namespace Ardium::HAL
