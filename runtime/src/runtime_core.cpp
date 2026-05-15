// lib/runtime_core.cpp
// Minimal C++ bridge for Ardium runtime (<30% of runtime code)
// Only essential system calls and FFI - everything else in Ardium

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dlfcn.h>
#include <pthread.h>
#include <math.h>

// Prevent C++ name mangling for Ardium
extern "C" {

// ============================================================================
// MEMORY MANAGEMENT (ARC & Syscall Bridge)
// ============================================================================

// Internal struct to hold ARC metadata
typedef struct {
    uint32_t ref_count;
    uint32_t flags;
    char payload[];
} arc_header_t;

void* __sys_malloc(size_t size) {
    // Allocate extra space for ARC header
    arc_header_t* header = (arc_header_t*)malloc(sizeof(arc_header_t) + size);
    if (!header) return NULL;
    header->ref_count = 1; // Default to 1 on creation
    header->flags = 0;
    return header->payload;
}

void __sys_free(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = (arc_header_t*)((char*)ptr - sizeof(arc_header_t));
    free(header);
}

void __arc_retain(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = (arc_header_t*)((char*)ptr - sizeof(arc_header_t));
    header->ref_count += 1;
}

void __arc_release(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = (arc_header_t*)((char*)ptr - sizeof(arc_header_t));
    if (header->ref_count > 0) {
        header->ref_count -= 1;
        if (header->ref_count == 0) {
            free(header);
        }
    }
}

void* __sys_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

void* __sys_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* __sys_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

// ============================================================================
// ERROR HANDLING (Thread-Local State for MVP Try/Catch)
// ============================================================================

thread_local char* __ardium_last_error = NULL;

void __sys_throw(const char* err) {
    if (__ardium_last_error) {
        free(__ardium_last_error);
    }
    if (err) {
        __ardium_last_error = strdup(err);
    } else {
        __ardium_last_error = strdup("Unknown Error");
    }
}

const char* __sys_get_error() {
    return __ardium_last_error;
}

void __sys_clear_error() {
    if (__ardium_last_error) {
        free(__ardium_last_error);
        __ardium_last_error = NULL;
    }
}

// ============================================================================
// FILE I/O (Syscall wrappers only)
// ============================================================================

int __sys_open(const char* path, int flags, int mode) {
    return open(path, flags, mode);
}

int __sys_close(int fd) {
    return close(fd);
}

ssize_t __sys_read(int fd, void* buf, size_t count) {
    return read(fd, buf, count);
}

ssize_t __sys_write(int fd, const void* buf, size_t count) {
    return write(fd, buf, count);
}

off_t __sys_lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

// ============================================================================
// DYNAMIC LOADING (FFI support)
// ============================================================================

void* __sys_dlopen(const char* filename, int flag) {
    return dlopen(filename, flag);
}

void* __sys_dlsym(void* handle, const char* symbol) {
    return dlsym(handle, symbol);
}

int __sys_dlclose(void* handle) {
    return dlclose(handle);
}

char* __sys_dlerror() {
    return dlerror();
}

// ============================================================================
// THREADING (pthread bridge only)
// ============================================================================

int __sys_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start_routine)(void*), void* arg) {
    return pthread_create(thread, attr, start_routine, arg);
}

int __sys_pthread_join(pthread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

void __sys_pthread_exit(void* retval) {
    pthread_exit(retval);
}

// ============================================================================
// SYSTEM INFO
// ============================================================================

int __sys_getpid() {
    return getpid();
}

// ============================================================================
// PRINT (Temporary - will be fully in Ardium later)
// ============================================================================

void __sys_print(const char* str) {
    write(1, str, strlen(str));
}

void __sys_println(const char* str) {
    write(1, str, strlen(str));
    write(1, "\n", 1);
}

void __sys_print_int(long n) {
    char buf[32]; // Enough for 64-bit int
    snprintf(buf, 32, "%ld\n", n);
    write(1, buf, strlen(buf));
}

// ============================================================================
// STRING & MATH (Intrinsics)
// ============================================================================

size_t __sys_strlen(const char* s) {
    // Basic wrapper
    const char* p = s;
    while (*p) p++;
    return p - s;
}

int __sys_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

double __sys_pow(double base, double exp) {
    return pow(base, exp);
}

// ============================================================================
// PROPERTY WRAPPERS (Reactivity bridge)
// ============================================================================

// String Concatenation Support
char* ardium_strcat(const char* s1, const char* s2) {
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = (char*)__sys_malloc(len1 + len2 + 1); // Use ARC malloc
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

} // extern "C"

// Total: ~120 lines - minimal bridge only
// All logic, data structures, algorithms -> Ardium
