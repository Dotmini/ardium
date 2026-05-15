#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Vision/Vision.h>
#import <AVFoundation/AVFoundation.h>
#include <dlfcn.h>
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// --- MAJOR ARCHITECT: Ardium v4.0 Runtime (Objective-C++) ---
// Mission: Super-Performance (Metal GPU, CoreML AI, Hot-Reloading).

// --- 1. Data Structures (Shared CPU/GPU) ---

struct Particle {
    float x, y;
    float vx, vy;
    float r, g, b, a;
};

struct MetalParticleSystem {
    id<MTLBuffer> buffer; // Zero-copy shared buffer
    Particle* raw_ptr;    // CPU access
    size_t count;
    float width, height;

    MetalParticleSystem(id<MTLDevice> device, size_t n, float w, float h) : count(n), width(w), height(h) {
        size_t size = n * sizeof(Particle);
        // Use Managed for Mac (CPU writes, GPU reads frequently) or Shared for Apple Silicon (Unified)
        // Shared is best for M1/M2/M3.
        buffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
        raw_ptr = (Particle*)buffer.contents;

        // Init
        for(size_t i=0; i<n; ++i) {
            raw_ptr[i].x = (rand() % (int)w);
            raw_ptr[i].y = (rand() % (int)h);
            raw_ptr[i].vx = ((rand()%100)/50.0f) - 1.0f;
            raw_ptr[i].vy = ((rand()%100)/50.0f) - 1.0f;
            raw_ptr[i].r = 0.0f; raw_ptr[i].g = 0.5f; raw_ptr[i].b = 1.0f; raw_ptr[i].a = 1.0f;
        }
        std::cout << "[Metal] Allocated Shared Buffer for " << n << " particles (" << size/1024/1024 << " MB).\n";
    }
};

static id<MTLDevice> mtl_device;
static id<MTLCommandQueue> mtl_command_queue;
static id<MTLRenderPipelineState> mtl_pipeline;
static std::mutex sys_mutex;
static std::unordered_map<int64_t, std::unique_ptr<MetalParticleSystem>> metal_systems;
static std::atomic<int64_t> sys_id_counter{1};

// --- 2. Vision / CoreML Bridge ---

@interface ArdiumVision : NSObject
@property (atomic) float faceX;
@property (atomic) float faceY;
@property (atomic) BOOL hasFace;
@end

@implementation ArdiumVision
- (instancetype)init {
    self = [super init];
    _faceX = 0.5; _faceY = 0.5; _hasFace = NO;
    return self;
}

// Real-time Face Tracking (Simulated for Headless, Logic Ready for Camera)
// In a real app, this would be a delegate for AVCaptureVideoDataOutput
- (void)detectFaces {
    // For this demo, we simulate "Real AI" by moving the attractor in a figure-8 pattern
    // to prove the physics engine responds to dynamic input.
    // Ideally, we'd process a CMSampleBuffer here with VNDetectFaceRectanglesRequest.

    double t = CFAbsoluteTimeGetCurrent();
    _faceX = 0.5 + sin(t) * 0.3;
    _faceY = 0.5 + cos(t * 0.7) * 0.3;
    _hasFace = YES;
}
@end

static ArdiumVision* global_vision;

// --- 3. Metal View Delegate ---

@interface ArdiumDelegate : NSObject <MTKViewDelegate>
@property (assign) int64_t systemId;
@end

@implementation ArdiumDelegate

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    // Handle resize
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    // [1] AI Update
    [global_vision detectFaces];
    float fx = global_vision.faceX;
    float fy = global_vision.faceY;

    // [2] Physics Update (CPU Multithreaded on Shared Memory)
    std::lock_guard<std::mutex> lock(sys_mutex);
    auto it = metal_systems.find(_systemId);
    if (it == metal_systems.end()) return;
    auto& sys = it->second;

    float width = sys->width;
    float height = sys->height;

    // Dispatch Apply for High Performance Physics (1M particles)
    dispatch_apply(sys->count, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^(size_t i) {
        Particle& p = sys->raw_ptr[i];

        // Integrate
        p.x += p.vx;
        p.y += p.vy;

        // Face Attraction (Gravity)
        float dx = (fx * width) - p.x;
        float dy = (fy * height) - p.y; // Invert Y for logic if needed, but keeping consistent
        float distSq = dx*dx + dy*dy;

        if (distSq < 200000) { // Attraction Range
            float force = 100.0f / (distSq + 1.0f); // Inverse squareish
            if (force > 0.5f) force = 0.5f; // Clamp
            p.vx += dx * force * 0.01f;
            p.vy += dy * force * 0.01f;
            p.r = 1.0f; p.g = 0.2f; p.b = 0.2f; // Red
        } else {
            p.r = 0.0f; p.g = 0.5f; p.b = 1.0f; // Blue
            // Damping
            p.vx *= 0.99f;
            p.vy *= 0.99f;
        }

        // Bounds Bounce
        if (p.x < 0) { p.x = 0; p.vx *= -1; }
        if (p.x > width) { p.x = width; p.vx *= -1; }
        if (p.y < 0) { p.y = 0; p.vy *= -1; }
        if (p.y > height) { p.y = height; p.vy *= -1; }
    });

    // [3] Metal Render
    id<MTLCommandBuffer> commandBuffer = [mtl_command_queue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;

    if(renderPassDescriptor != nil) {
        // Clear Color (Dark Grey)
        renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setRenderPipelineState:mtl_pipeline];

        // Bind Shared Buffer
        [renderEncoder setVertexBuffer:sys->buffer offset:0 atIndex:0];

        // Bind Viewport Uniform
        float viewport[2] = {width, height};
        [renderEncoder setVertexBytes:viewport length:sizeof(viewport) atIndex:1];

        // Draw 1 Million Points
        [renderEncoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:sys->count];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:view.currentDrawable];
    }
    [commandBuffer commit];
}
@end

// --- 4. Hot Reloading Logic ---

typedef int (*ArdiumTickFunc)(int64_t);
static void* dylib_handle = NULL;
static ArdiumTickFunc current_tick_func = NULL;

// Global Delegate Reference to keep it alive
static ArdiumDelegate* global_delegate;

extern "C" {

    // [Metal] Init
    void coreui_init_metal() {
        mtl_device = MTLCreateSystemDefaultDevice();
        if(!mtl_device) { std::cout << "[Metal] Failed to get device.\n"; return; }
        mtl_command_queue = [mtl_device newCommandQueue];

        // Load default library (compiled shaders.metallib)
        NSError* err = nil;

        // Try loading from file first
        id<MTLLibrary> lib = [mtl_device newLibraryWithFile:@"default.metallib" error:&err];

        // Fallback to embedded source if file load fails
        if(!lib) {
            std::cout << "[Metal] 'default.metallib' not found or failed. Compiling embedded source...\n";
            // Define source string here since we can't depend on the external file
            const char* METAL_SHADERS_SRC =
            "using namespace metal;\n"
            "struct Particle { float x; float y; float vx; float vy; float r; float g; float b; float a; };\n"
            "struct VertexOut { float4 position [[position]]; float4 color; float pointSize [[point_size]]; };\n"
            "vertex VertexOut particle_vertex(uint vertexID [[vertex_id]], const device Particle* particles [[buffer(0)]], constant float2& viewportSize [[buffer(1)]]) {\n"
            "    VertexOut out;\n"
            "    Particle p = particles[vertexID];\n"
            "    float x = (p.x / viewportSize.x) * 2.0 - 1.0;\n"
            "    float y = (1.0 - (p.y / viewportSize.y)) * 2.0 - 1.0;\n"
            "    out.position = float4(x, y, 0.0, 1.0);\n"
            "    out.color = float4(p.r, p.g, p.b, p.a);\n"
            "    out.pointSize = 2.0;\n"
            "    return out;\n"
            "}\n"
            "fragment float4 particle_fragment(VertexOut in [[stage_in]]) { return in.color; }\n";

            lib = [mtl_device newLibraryWithSource:[NSString stringWithUTF8String:METAL_SHADERS_SRC] options:nil error:&err];
        }

        if(!lib) {
            std::cout << "[Metal] Shader Compile Error: " << [[err localizedDescription] UTF8String] << "\n";
            return;
        }

        MTLRenderPipelineDescriptor *pDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pDesc.vertexFunction = [lib newFunctionWithName:@"particle_vertex"];
        pDesc.fragmentFunction = [lib newFunctionWithName:@"particle_fragment"];
        pDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        mtl_pipeline = [mtl_device newRenderPipelineStateWithDescriptor:pDesc error:&err];
        if(!mtl_pipeline) std::cout << "[Metal] Pipeline Error: " << [[err localizedDescription] UTF8String] << "\n";
        else std::cout << "[Metal] GPU Pipeline Initialized.\n";

        global_vision = [[ArdiumVision alloc] init];
    }

    // [Metal] Create System
    int64_t metal_create_system(int64_t count, double w, double h) {
        std::lock_guard<std::mutex> lock(sys_mutex);
        int64_t id = sys_id_counter++;
        metal_systems[id] = std::make_unique<MetalParticleSystem>(mtl_device, (size_t)count, (float)w, (float)h);
        return id;
    }

    // [UI] Create Metal Window
    void coreui_create_metal_window(const char* title, int64_t sys_id) {
        @autoreleasepool {
            NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1024, 768)
                                                           styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                             backing:NSBackingStoreBuffered defer:NO];
            [window setTitle:[NSString stringWithUTF8String:title]];

            MTKView* mtkView = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, 1024, 768)];
            mtkView.device = mtl_device;
            mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
            mtkView.clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);
            mtkView.preferredFramesPerSecond = 120; // High Refresh Rate

            global_delegate = [[ArdiumDelegate alloc] init];
            global_delegate.systemId = sys_id;
            mtkView.delegate = global_delegate;

            [window setContentView:mtkView];
            [window makeKeyAndOrderFront:nil];
            [window center];

            std::cout << "[UI] Metal Window Created (120Hz).\n";
        }
    }

    // [Hot Reload] Load Logic
    void coreui_load_logic(const char* path) {
        if (dylib_handle) dlclose(dylib_handle);
        dylib_handle = dlopen(path, RTLD_NOW);
        if (dylib_handle) {
            current_tick_func = (ArdiumTickFunc)dlsym(dylib_handle, "tick");
            std::cout << "[Hot-Reload] Logic Loaded.\n";
        } else {
            std::cout << "[Hot-Reload] Error: " << dlerror() << "\n";
        }
    }

    // Run Loop
    void coreui_run_loop() {
        [NSApp run];
    }
}
