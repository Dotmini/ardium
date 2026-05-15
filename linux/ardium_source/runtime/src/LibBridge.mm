#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <vector>

// --- MEMORY CONTROLLER EXTERNS ---
// We use the safe memory logger from Module 1 if available, otherwise fallback to stderr
namespace Ardium::Memory {
    extern void Log(const char* fmt, ...);
}

// Helper for exception safety
// Returns 0 on success, -1 on error.
// For functions returning values, we might need a specific protocol or valid defaults.
#define SAFE_EXEC(block) \
    try { \
        block \
    } catch (const std::exception& e) { \
        std::cerr << "[LibBridge] Exception: " << e.what() << std::endl; \
        return -1; \
    } catch (...) { \
        std::cerr << "[LibBridge] Unknown Exception." << std::endl; \
        return -1; \
    }

extern "C" {

    // --- IO ---

    // Prints a string to stdout.
    // Returns 0 on success.
    int64_t core_print(const char* str) {
        SAFE_EXEC({
            if (str) {
                std::cout << str << std::endl;
                return 0;
            } else {
                return -1; // Null pointer
            }
        });
        return -1;
    }

    // Reads an entire file into a heap-allocated buffer.
    // Returns pointer to the string data (char*), or nullptr on failure.
    // NOTE: The caller (Ardium) is responsible for freeing this memory if ownership is transferred,
    // or we rely on the MemoryController to track it. For simple bridge, we use malloc so Ardium's 'free' can handle it.
    char* core_file_read(const char* path) {
        try {
            if (!path) return nullptr;
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open()) return nullptr;

            size_t size = file.tellg();
            file.seekg(0);

            // Allocate buffer (+1 for null terminator)
            char* buffer = (char*)malloc(size + 1);
            if (!buffer) return nullptr;

            file.read(buffer, size);
            buffer[size] = '\0'; // Null-terminate string

            return buffer;
        } catch (...) {
            std::cerr << "[LibBridge] File Read Error." << std::endl;
            return nullptr;
        }
    }

    // --- MATH ---

    // Returns sin(x) as double
    double core_sin(double x) {
        try { return std::sin(x); } catch(...) { return 0.0; }
    }

    // Returns cos(x) as double
    double core_cos(double x) {
        try { return std::cos(x); } catch(...) { return 0.0; }
    }

    // Returns a random integer between min and max (inclusive)
    int64_t core_random(int64_t min, int64_t max) {
        try {
            // Using static thread_local engine for performance and safety
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_int_distribution<int64_t> distribution(min, max);
            return distribution(generator);
        } catch (...) {
            return min; // Fail safe
        }
    }

    // --- SYSTEM ---

    // Returns current time in milliseconds since epoch
    int64_t core_get_time() {
        try {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            return (int64_t)ms;
        } catch(...) { return 0; }
    }

    // Sleeps for the specified number of milliseconds
    int64_t core_sleep(int64_t ms) {
        SAFE_EXEC({
            if (ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            return 0;
        });
        return -1;
    }
}
