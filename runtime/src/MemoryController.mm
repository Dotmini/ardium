#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <array>
#include <cstdarg>
#include <cstring>
#include <algorithm>

// Mach VM for macOS
#import <mach/mach_vm.h>
#import <mach/mach_init.h>

/**
 * ============================================================================ 
 *  ARDIUM TITAN RUNTIME - MODULE 1: MEMORY CONTROLLER & LOGGING
 * ============================================================================ 
 *  Architect: Major
 *  
 *  Description:
 *  Provides the "UnifiedMemoryPool" for managing large memory blocks.
 *  Uses `vm_allocate` on macOS for page-aligned, zero-filled memory.
 *  Includes a high-performance Thread-Safe Circular Buffer for logging.
 * ============================================================================ 
 */

namespace Ardium {
namespace Memory {

    // --- CONFIGURATION ---
    constexpr size_t LOG_BUFFER_SIZE = 1024 * 1024; // 1MB Log Buffer
    constexpr size_t PAGE_SIZE = 16384; // 16KB Page (Apple Silicon standard)

    // --- CIRCULAR LOG BUFFER ---
    class CircularLogBuffer {
        std::array<char, LOG_BUFFER_SIZE> buffer;
        size_t head = 0;
        std::mutex m_mtx;

    public:
        void append(const char* msg) {
            std::lock_guard<std::mutex> lock(m_mtx);
            size_t len = strlen(msg);
            
            // In a real circular buffer, we'd wrap around. 
            // For simplicity/safety in this log dump, we'll shift if full or truncate.
            // Let's implement true circular logic but print linear for now? 
            // Just protecting stdout is safer for this stage.
            
            // Actually, requirements ask for "Thread-Safe Circular Buffer".
            // Let's implement one that just overwrites old data if needed.
            
            for (size_t i = 0; i < len; ++i) {
                buffer[head] = msg[i];
                head = (head + 1) % LOG_BUFFER_SIZE;
            }
            // Add newline
            buffer[head] = '\n';
            head = (head + 1) % LOG_BUFFER_SIZE;
            
            // Also echo to stdout for dev visibility
            std::cout << msg << std::endl;
        }
    };

    static CircularLogBuffer global_log;

    // Exposed Log Function
    void Log(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        global_log.append(buf);
    }

    // --- UNIFIED MEMORY POOL ---
    class UnifiedMemoryPool {
        struct AllocationInfo {
            size_t size;
            bool is_vm; // True if vm_allocate was used
        };

        std::unordered_map<void*, AllocationInfo> m_allocations;
        std::atomic<size_t> m_total_allocated{0};
        std::atomic<size_t> m_peak_usage{0};
        std::mutex m_mtx;

    public:
        UnifiedMemoryPool() {
            Log("[MemoryController] Initializing Unified Memory Pool...");
        }

        ~UnifiedMemoryPool() {
            Log("[MemoryController] Shutting down. Checking leaks...");
            std::lock_guard<std::mutex> lock(m_mtx);
            if (!m_allocations.empty()) {
                Log("[MemoryController] LEAK DETECTED! %zu blocks remaining.", m_allocations.size());
                for (const auto& [ptr, info] : m_allocations) {
                    Log(" -> Leaked Block: %p (%zu bytes)", ptr, info.size);
                    // In production, we might force dealloc here.
                }
            } else {
                Log("[MemoryController] Clean shutdown. No leaks.");
            }
        }

        void* alloc(size_t size) {
            void* ptr = nullptr;
            bool use_vm = size >= PAGE_SIZE; // Use VM for large blocks

            if (use_vm) {
                // macOS High-Performance VM Allocation
                mach_vm_address_t addr = 0;
                kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr, size, VM_FLAGS_ANYWHERE);
                if (kr != KERN_SUCCESS) {
                    Log("[MemoryController] vm_allocate failed! (Size: %zu)", size);
                    return nullptr;
                }
                ptr = (void*)addr;
            } else {
                // Standard malloc for small objects
                ptr = std::malloc(size);
            }

            if (ptr) {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_allocations[ptr] = {size, use_vm};
                m_total_allocated += size;
                
                size_t current = m_total_allocated.load();
                size_t peak = m_peak_usage.load();
                if (current > peak) m_peak_usage.store(current);
                
                Log("[MemoryController] Alloc %p (%zu bytes) [VM: %d] Total: %zu", ptr, size, use_vm, current);
            }
            return ptr;
        }

        void dealloc(void* ptr) {
            if (!ptr) return;

            std::lock_guard<std::mutex> lock(m_mtx);
            auto it = m_allocations.find(ptr);
            if (it != m_allocations.end()) {
                size_t size = it->second.size;
                bool is_vm = it->second.is_vm;

                if (is_vm) {
                    mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)ptr, size);
                } else {
                    std::free(ptr);
                }

                m_total_allocated -= size;
                m_allocations.erase(it);
                Log("[MemoryController] Free %p (%zu bytes). Total: %zu", ptr, size, m_total_allocated.load());
            } else {
                Log("[MemoryController] Attempt to free unknown pointer: %p", ptr);
            }
        }

        size_t get_total_usage() const { return m_total_allocated.load(); }
        size_t get_peak_usage() const { return m_peak_usage.load(); }
    };

    // Singleton Instance
    static UnifiedMemoryPool* g_pool = nullptr;

    void Init() {
        if (!g_pool) g_pool = new UnifiedMemoryPool();
    }

    void Shutdown() {
        if (g_pool) {
            delete g_pool;
            g_pool = nullptr;
        }
    }

    void* Malloc(size_t size) {
        if (g_pool) return g_pool->alloc(size);
        return std::malloc(size); // Fallback 
    }

    void Free(void* ptr) {
        if (g_pool) g_pool->dealloc(ptr);
        else std::free(ptr); // Fallback 
    }

} // namespace Memory
} // namespace Ardium
