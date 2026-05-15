#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cmath>
#include <random>

// --- MAJOR ARCHITECT: Ardium v3.2 Runtime (Objective-C++) ---
// Mission: Hybrid Multi-threaded Physics & AI Engine with Native Visualization.

// --- 1. Data Structures (Ownership Managed) ---

struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b, a; // Color
};

struct ParticleSystem {
    std::vector<Particle> particles;
    float width, height;

    ParticleSystem(int count, float w, float h) : width(w), height(h) {
        particles.resize(count);
        std::mt19937 rng(42); // Deterministic seed
        std::uniform_real_distribution<float> distX(0, w);
        std::uniform_real_distribution<float> distY(0, h);
        std::uniform_real_distribution<float> distV(-2.0, 2.0);
        
        for(auto& p : particles) {
            p.x = distX(rng);
            p.y = distY(rng);
            p.vx = distV(rng);
            p.vy = distV(rng);
            p.r = 0.0f; p.g = 0.5f; p.b = 1.0f; p.a = 1.0f; // Ardium Blue
        }
        std::cout << "[Ownership] Allocated ParticleSystem with " << count << " particles.\n";
    }
};

static std::mutex data_mutex;
static std::unordered_map<int64_t, std::unique_ptr<ParticleSystem>> systems_store;
static std::atomic<int64_t> system_id_counter{1};

// --- 2. Visualization (ARC / Cocoa) ---

@interface ArdiumCanvas : NSView
@property (assign) int64_t systemId;
@end

@implementation ArdiumCanvas
- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);

    // [Hybrid Bridge] Access owned data safely
    // For demo speed, we lock briefly to get pointer, but in real-time sim,
    // we might accept tearing or double buffer. Here we lock to prevent crash on destruction.
    std::lock_guard<std::mutex> lock(data_mutex);
    
    auto it = systems_store.find(_systemId);
    if (it != systems_store.end()) {
        auto& sys = it->second;
        CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
        
        for (const auto& p : sys->particles) {
            CGContextSetRGBFillColor(context, p.r, p.g, p.b, p.a);
            CGContextFillEllipseInRect(context, CGRectMake(p.x - 3, p.y - 3, 6, 6));
        }
    }
}
@end

// View Store
static std::unordered_map<int64_t, id> view_store;
static int64_t view_id_counter = 1;

// --- 3. Logic Kernels (C++20) ---

// Forward declaration of Ardium callback
extern "C" void on_tick(); 

extern "C" {

    // [API] Create Particle System
    int64_t coreui_create_system(int64_t count, double w, double h) {
        std::lock_guard<std::mutex> lock(data_mutex);
        int64_t id = system_id_counter++;
        systems_store[id] = std::make_unique<ParticleSystem>(count, (float)w, (float)h);
        return id;
    }

    // [Kernel] Physics Update (Parallel Data)
    void coreui_update_physics(int64_t sys_id) {
        ParticleSystem* sys = nullptr;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (systems_store.find(sys_id) != systems_store.end()) {
                sys = systems_store[sys_id].get();
            }
        }
        
        if (!sys) return;

        // GCD Parallel For (High Throughput)
        dispatch_apply(sys->particles.size(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(size_t i) {
            Particle& p = sys->particles[i];
            
            // Integrate
            p.x += p.vx;
            p.y += p.vy;
            
            // Bounce (Boundaries)
            if (p.x < 0) { p.x = 0; p.vx *= -1; }
            if (p.x > sys->width) { p.x = sys->width; p.vx *= -1; }
            if (p.y < 0) { p.y = 0; p.vy *= -1; }
            if (p.y > sys->height) { p.y = sys->height; p.vy *= -1; }
        });
    }

    // [Kernel] AI Inference (Async Background)
    // Simulates Neural Network deciding particle color based on velocity/position
    void coreui_run_ai(int64_t sys_id) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{ //
            ParticleSystem* sys = nullptr;
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                if (systems_store.find(sys_id) != systems_store.end()) {
                    sys = systems_store[sys_id].get();
                }
            }
            if (!sys) return;

            // Simple heuristic to simulate AI inference load
            for (auto& p : sys->particles) {
                float speed = std::sqrt(p.vx*p.vx + p.vy*p.vy);
                // AI Logic: High energy = Red, Low energy = Blue
                float energy = std::min(speed / 3.0f, 1.0f);
                p.r = energy;
                p.g = 0.5f * (1.0f - energy);
                p.b = 1.0f - energy;
            }
        });
    }

    // [UI] Create Canvas
    int64_t coreui_create_canvas(int64_t sys_id, double w, double h) {
        @autoreleasepool {
            ArdiumCanvas* canvas = [[ArdiumCanvas alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
            canvas.systemId = sys_id;
            int64_t vid = view_id_counter++;
            view_store[vid] = canvas;
            return vid;
        }
    }

    // [UI] Redraw Canvas (Sync Main)
    void coreui_redraw_canvas(int64_t vid) {
        dispatch_async(dispatch_get_main_queue(), ^{ //
            id view = view_store[vid];
            if (view) [view setNeedsDisplay:YES];
        });
    }

    // [UI] Init App
    void coreui_init_app() {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }

    // [UI] Create Window with Hosting View
    int64_t coreui_create_host_window(const char* title, int64_t canvas_vid) {
        @autoreleasepool {
            NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
                                                           styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                             backing:NSBackingStoreBuffered defer:NO];
            [window setTitle:[NSString stringWithUTF8String:title]];
            
            // Simple Layout: Split View simulation
            NSView* contentView = [window contentView];
            NSView* canvas = view_store[canvas_vid];
            
            // Left Panel (Control Placeholder) - Gray
            NSView* controls = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 200, 600)];
            controls.wantsLayer = YES;
            controls.layer.backgroundColor = [[NSColor colorWithWhite:0.2 alpha:1.0] CGColor];
            [contentView addSubview:controls];
            
            // Right Panel (Canvas)
            if (canvas) {
                [canvas setFrame:NSMakeRect(200, 0, 600, 600)];
                [contentView addSubview:canvas];
            }

            [window makeKeyAndOrderFront:nil];
            [window center];
            
            int64_t vid = view_id_counter++;
            view_store[vid] = window;
            return vid;
        }
    }

    // [Timer] Loop Driver
    void coreui_start_timer(double ms) {
        NSTimer* timer = [NSTimer timerWithTimeInterval:(ms / 1000.0)
                                                repeats:YES
                                                  block:^(NSTimer * _Nonnull timer) {
            // Call back into Ardium
            on_tick();
        }];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    }

    void coreui_run_loop() {
        std::cout << "[Runtime] Entering Main Loop...\n";
        [NSApp run];
    }
}
