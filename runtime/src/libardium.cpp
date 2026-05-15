// runtime/src/libardium.cpp
// Ardium v2.5.6 Production Runtime
// Implements robust primitives for Strings, Memory, and IO.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

extern "C" {

    // --- Error Handling ---
    void ardium_signal_handler(int signal) {
        if (signal == SIGSEGV) {
            std::fprintf(stderr, "\n\033[1;31m🔥 Ardium Runtime Error: Segmentation Fault (139)\033[0m\n");
            std::fprintf(stderr, "   The memory goblins ate your data.\n");
            std::fprintf(stderr, "   Check your pointers, array bounds, and external calls.\n");
            std::exit(139);
        }
    }

    void ardium_init_runtime() {
        signal(SIGSEGV, ardium_signal_handler);
    }

    // --- Memory Management ---
    void* ardium_malloc(int64_t size) {
        if (size <= 0) return nullptr;
        void* ptr = std::malloc(size);
        if (!ptr) {
            std::cerr << "[Ardium Runtime] Out of Memory" << std::endl;
            std::exit(1);
        }
        return ptr;
    }

    void ardium_free(void* ptr) {
        if (ptr) std::free(ptr);
    }

    // --- String Operations ---
    // Safe string concatenation using std::string for intermediate storage
    char* ardium_str_concat(const char* s1, const char* s2) {
        if (!s1 || !s2) return nullptr; // Handle null safety
        
        std::string res = std::string(s1) + std::string(s2);
        
        // Allocate C-string for return (Ardium owns the memory)
        char* out = (char*)ardium_malloc(res.length() + 1);
        std::strcpy(out, res.c_str());
        return out;
    }

    int64_t ardium_strlen(const char* s) {
        if (!s) return 0;
        return std::strlen(s);
    }

    int32_t ardium_streq(const char* s1, const char* s2) {
        if (!s1 || !s2) return 0;
        return std::strcmp(s1, s2) == 0 ? 1 : 0;
    }

    // --- System (Type Safe) ---
    void ardium_print_str(const char* msg) { if (msg) std::printf("%s", msg); }
    void ardium_println_str(const char* msg) { if (msg) std::printf("%s\n", msg); else std::printf("\n"); }

    void ardium_print_int(int64_t val) { std::printf("%lld", val); }
    void ardium_println_int(int64_t val) { std::printf("%lld\n", val); }

    void ardium_print_double(double val) { std::printf("%.6f", val); }
    void ardium_println_double(double val) { std::printf("%.6f\n", val); }
    
    // Legacy mapping (generic for now maps to str)
    void ardium_println(const char* msg) { ardium_println_str(msg); }
    void ardium_print(const char* msg) { ardium_print_str(msg); }
}
