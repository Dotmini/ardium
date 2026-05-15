#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cmath>

// --- MAJOR ARCHITECT: Ardium v3.1 Runtime (Objective-C++) ---
// Mission: Native Multi-threading via GCD & C++20.
// Strategy: "No UI Lag, Maximum Throughput".

// --- 1. Memory Management Systems (Enhanced for MT) ---

// [Ownership] Data Store (Thread-Safe RAII)
struct DataBuffer {
    std::unique_ptr<uint8_t[]> raw_bytes;
    size_t size;
    DataBuffer(size_t s) : size(s) { raw_bytes = std::make_unique<uint8_t[]>(s); }
    ~DataBuffer() { 
        // std::cout << "[Ownership] Deallocating DataBuffer (RAII).\n"; 
    }
};

// [Ownership] Image Buffer (Thread-Safe)
struct ImageBuffer {
    std::unique_ptr<uint32_t[]> pixels;
    int64_t width;
    int64_t height;

    ImageBuffer(int64_t w, int64_t h) : width(w), height(h) {
        pixels = std::make_unique<uint32_t[]>(w * h);
        std::cout << "[Ownership] Allocated ImageBuffer (" << w << "x" << h << ") on Thread: " << std::this_thread::get_id() << "\n";
    }
};

static std::mutex ownership_mutex;
static std::unordered_map<int64_t, std::unique_ptr<DataBuffer>> ownership_store;
static std::unordered_map<int64_t, std::unique_ptr<ImageBuffer>> image_store;
static std::atomic<int64_t> ownership_id_counter{1};

// [ARC] UI View Store (Main Thread Only)
static std::unordered_map<int64_t, id> view_store;
static int64_t view_id_counter = 1;

// --- 2. Internal Helpers ---

// Task Registry for "Function Pointer" simulation (since Ardium v3.0 ABI doesn't support raw fn pointers yet)
typedef void (*TaskFunc)(int64_t);
static std::unordered_map<int64_t, TaskFunc> task_registry;

// --- 3. Runtime Interface (Extern "C") ---

extern "C" {

    // --- CONCURRENCY API (GCD + C++20) ---

    // 1. Parallel For (Data Parallelism)
    // Distributes loop iterations across all performance cores using dispatch_apply.
    void coreui_parallel_for(int64_t start, int64_t end, int64_t task_id) {
        if (task_registry.find(task_id) == task_registry.end()) {
            std::cerr << "[MT-Error] Unknown Task ID: " << task_id << "\n";
            return;
        }
        
        TaskFunc func = task_registry[task_id];
        size_t count = (size_t)(end - start);
        
        // High-performance concurrent queue
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
        
        // Block until complete (Apply is synchronous but parallel)
        dispatch_apply(count, queue, ^(size_t i) {
            func(start + (int64_t)i);
        });
    }

    // 2. Async Background Task
    // Offloads work to background, returns immediately.
    void coreui_async(int64_t task_id) {
        if (task_registry.find(task_id) == task_registry.end()) return;
        TaskFunc func = task_registry[task_id];
        
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{ 
            // std::cout << "[GCD] Starting Async Task on Thread: " << std::this_thread::get_id() << "\n";
            func(0); // Argument 0 for void tasks
        });
    }

    // 3. Sync to Main Thread (UI Updates)
    // Critical for updating UI from background threads.
    void coreui_sync_main(int64_t task_id) {
        if (task_registry.find(task_id) == task_registry.end()) return;
        TaskFunc func = task_registry[task_id];
        
        dispatch_async(dispatch_get_main_queue(), ^{ 
            func(0);
        });
    }

    // --- Pre-defined High Performance Tasks (For Demo) ---
    // In a future ABI, these would be passed dynamically.
    
    // Task 1: Heavy Math (CPU Burn)
    void task_cpu_burn(int64_t i) {
        volatile double x = 0.0;
        for(int j=0; j<1000; j++) { x += std::sin(i * j * 0.01); }
    }

    // Task 2: Image Processing (Gradient)
    // Assumes Image ID 1 exists.
    void task_image_gradient(int64_t i) {
        // Safe concurrent access: 'i' is the row index (y)
        // We only need read access to the map pointer, but write access to the pixel data.
        // Since threads write to disjoint memory rows, this is safe without locking per pixel.
        
        ImageBuffer* img = nullptr;
        {
            // Minimal lock scope just to get the pointer
            std::lock_guard<std::mutex> lock(ownership_mutex);
            if (image_store.find(1) != image_store.end()) {
                img = image_store[1].get();
            }
        }

        if (img && i < img->height) {
            for (int64_t x = 0; x < img->width; ++x) {
                uint8_t r = (uint8_t)((x * 255) / img->width);
                uint8_t g = (uint8_t)((i * 255) / img->height);
                uint8_t b = (uint8_t)((std::sin(x * 0.01) + 1.0) * 127);
                img->pixels[i * img->width + x] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }

    // Task 3: Fibonacci Stress Test (Recursive)
    // Calculates Fib(n) where n is passed as the argument 'i'. 
    // To make it a loop stress test, we might ignore 'i' and do a fixed hard calc, 
    // or use 'i' as the input if we want variable load. 
    // Let's make it calculate Fib(35 + (i % 5)) to vary load slightly.
    long fib_recursive(long n) {
        if (n <= 1) return n;
        return fib_recursive(n - 1) + fib_recursive(n - 2);
    }

    void task_fib_stress(int64_t i) {
        // Simulating heavy work: Fib(30) is fast, Fib(40) is slow.
        // Let's do a moderately heavy calc per thread.
        long n = 35 + (i % 3); 
        long result = fib_recursive(n);
        // Print strictly necessary to verify work, but locked to prevent garbled output
        // std::lock_guard<std::mutex> lock(ownership_mutex);
        // std::cout << "[Thread " << std::this_thread::get_id() << "] Fib(" << n << ") = " << result << "\n";
        (void)result; // Prevent optimization
    }

    // Register tasks
    void coreui_register_tasks() {
        task_registry[100] = task_cpu_burn;
        task_registry[200] = task_image_gradient;
        task_registry[300] = task_fib_stress;
    }

    // --- Legacy v3.0 Ownership API ---
    int64_t image_own_buffer(int64_t width, int64_t height) {
        std::lock_guard<std::mutex> lock(ownership_mutex);
        int64_t id = ownership_id_counter++;
        image_store[id] = std::make_unique<ImageBuffer>(width, height);
        return id;
    }
    
    void image_drop(int64_t id) {
        std::lock_guard<std::mutex> lock(ownership_mutex);
        image_store.erase(id); // Destructor runs here
    }

    // --- Legacy v3.0 ARC API ---
    void coreui_init_app() {
        coreui_register_tasks(); // Initialize registry
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }

    int64_t coreui_create_window(const char* title, int64_t w, int64_t h) {
        @autoreleasepool {
            NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
                                                           styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                             backing:NSBackingStoreBuffered defer:NO];
            [window setTitle:[NSString stringWithUTF8String:title]];
            [window makeKeyAndOrderFront:nil];
            [window center];
            int64_t vid = view_id_counter++;
            view_store[vid] = window;
            return vid;
        }
    }

    void coreui_run_loop() {
        std::cout << "[Runtime] Entering Main Loop (GCD Active)...\n";
        [NSApp run];
    }
}
