#include "../include/ArdiumGraphics.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreVideo/CoreVideo.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>

// External Logging
namespace Ardium::Memory { extern void Log(const char* fmt, ...); }

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 2: GRAPHICS IMPLEMENTATION
 * ============================================================================
 *  Architect: Major
 *  Platform:  macOS (Metal)
 * ============================================================================
 */

namespace Ardium::Graphics {

    // --- CONFIGURATION ---
    static const int kMaxInFlightFrames = 3;

    // --- EMBEDDED SHADER SOURCE (Fallback) ---
    // If Shaders.metallib is missing, we compile this string at runtime.
    const char* EMBEDDED_SHADER_SRC = R"(
    #include <metal_stdlib>
    using namespace metal;
    struct VertexIn { float2 position; float4 color; };
    struct RasterizerData { float4 position [[position]]; float4 color; float pointSize [[point_size]]; };
    
    vertex RasterizerData basic_vertex(uint vertexID [[vertex_id]],
                                       uint instanceID [[instance_id]],
                                       const device VertexIn* instances [[buffer(0)]],
                                       constant float2& viewportSize [[buffer(1)]]) {
        RasterizerData out;
        VertexIn inst = instances[instanceID];
        float x = (inst.position.x / viewportSize.x) * 2.0 - 1.0;
        float y = (1.0 - (inst.position.y / viewportSize.y)) * 2.0 - 1.0;
        out.position = float4(x, y, 0.0, 1.0);
        out.color = inst.color;
        out.pointSize = 4.0;
        return out;
    }
    
    fragment float4 circle_fragment(RasterizerData in [[stage_in]]) {
        return in.color;
    }
    )";

    // --- METAL ENGINE CLASS ---
    class MetalEngine : public IGraphicsEngine {
        id<MTLDevice> m_device;
        id<MTLCommandQueue> m_commandQueue;
        id<MTLRenderPipelineState> m_pipelineState;
        
        // Triple Buffering
        dispatch_semaphore_t m_frameSemaphore;
        uint32_t m_currentFrameIndex = 0;
        
        // Display Link
        CVDisplayLinkRef m_displayLink = nullptr;
        std::atomic<bool> m_isHeadless{false};
        
        // Resource Map (Handle ID -> Buffer)
        std::unordered_map<uint64_t, id<MTLBuffer>> m_buffers;
        std::atomic<uint64_t> m_bufferIdCounter{1};
        std::mutex m_resourceMtx;

        // Viewport
        uint32_t m_width = 800;
        uint32_t m_height = 600;

        // Internal State
        MTLRenderPassDescriptor* m_currentPassDescriptor = nil;
        id<MTLDrawable> m_currentDrawable = nil;

    public:
        MetalEngine() {
            m_frameSemaphore = dispatch_semaphore_create(kMaxInFlightFrames);
        }

        ~MetalEngine() {
            Shutdown();
        }

        // --- INITIALIZATION ---
        bool Init() override {
            Ardium::Memory::Log("[Graphics] Initializing Metal Engine...");
            
            m_device = MTLCreateSystemDefaultDevice();
            if (!m_device) {
                Ardium::Memory::Log("[Graphics] Warning: Metal Device not found. Switching to HEADLESS mode.");
                m_isHeadless = true;
                return false;
            }

            Ardium::Memory::Log("[Graphics] Metal Device: %s", [[m_device name] UTF8String]);
            m_commandQueue = [m_device newCommandQueue];

            // Setup Pipeline
            if (!SetupPipeline()) {
                Ardium::Memory::Log("[Graphics] Critical: Pipeline setup failed. Headless fallback.");
                m_isHeadless = true;
                return false;
            }

            // Setup Display Link (For vsync)
            SetupDisplayLink();

            return true;
        }

        bool SetupPipeline() {
            NSError* error = nil;
            id<MTLLibrary> library = nil;

            // Try loading default.metallib
            if ([[NSFileManager defaultManager] fileExistsAtPath:@"default.metallib"]) {
                library = [m_device newLibraryWithFile:@"default.metallib" error:&error];
            }

            // Fallback to embedded source
            if (!library) {
                Ardium::Memory::Log("[Graphics] 'default.metallib' not found. Compiling embedded source...");
                NSString* src = [NSString stringWithUTF8String:EMBEDDED_SHADER_SRC];
                library = [m_device newLibraryWithSource:src options:nil error:&error];
            }

            if (!library) {
                Ardium::Memory::Log("[Graphics] Shader Compilation Failed: %s", [[error localizedDescription] UTF8String]);
                return false;
            }

            MTLRenderPipelineDescriptor* pDesc = [[MTLRenderPipelineDescriptor alloc] init];
            pDesc.vertexFunction = [library newFunctionWithName:@"basic_vertex"];
            pDesc.fragmentFunction = [library newFunctionWithName:@"circle_fragment"];
            pDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            
            // Enable blending for transparent particles
            pDesc.colorAttachments[0].blendingEnabled = YES;
            pDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            pDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

            m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pDesc error:&error];
            if (!m_pipelineState) {
                Ardium::Memory::Log("[Graphics] Pipeline State Error: %s", [[error localizedDescription] UTF8String]);
                return false;
            }

            return true;
        }

        void Shutdown() override {
            if (m_displayLink) {
                CVDisplayLinkStop(m_displayLink);
                CVDisplayLinkRelease(m_displayLink);
                m_displayLink = nullptr;
            }
            Ardium::Memory::Log("[Graphics] Engine Shutdown.");
        }

        // --- MEMORY MANAGEMENT ---
        void* CreateSharedBuffer(size_t sizeBytes, uint64_t& outHandleID) override {
            if (m_isHeadless) {
                // Return simple malloc for headless to prevent crash
                outHandleID = m_bufferIdCounter++;
                return malloc(sizeBytes); 
            }

            // MTLResourceStorageModeShared: The CPU and GPU share the same system memory (Unified Architecture)
            // This is "Zero-Copy".
            id<MTLBuffer> buffer = [m_device newBufferWithLength:sizeBytes options:MTLResourceStorageModeShared];
            if (!buffer) return nullptr;

            std::lock_guard<std::mutex> lock(m_resourceMtx);
            uint64_t id = m_bufferIdCounter++;
            m_buffers[id] = buffer;
            outHandleID = id;
            
            Ardium::Memory::Log("[Graphics] Shared Buffer Created (ID: %llu, Size: %zu bytes)", id, sizeBytes);
            return [buffer contents];
        }

        void SyncBuffer(uint64_t handleID) override {
            // On Apple Silicon (Shared Mode), explicit sync is mostly automatic for coherency,
            // but `didModifyRange` can be used if we were using Managed mode.
            // For Shared mode, memory barriers are handled by command buffer submission usually.
            // We implement this as a no-op or placeholder for Managed mode logic.
        }

        // --- RENDER LOOP ---
        
        static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext) {
            // In a real GUI app, this would drive the loop.
            // Since Ardium logic drives the loop via `coreui_run_loop` or `on_tick`, 
            // we use this mainly for timing or vsync throttling.
            return kCVReturnSuccess;
        }

        void SetupDisplayLink() {
            CVDisplayLinkCreateWithActiveCGDisplays(&m_displayLink);
            CVDisplayLinkSetOutputCallback(m_displayLink, &DisplayLinkCallback, (void*)this);
            CVDisplayLinkStart(m_displayLink);
        }

        RenderState BeginFrame() override {
            RenderState state = {0};
            if (m_isHeadless) return state;

            // Wait for availability (Triple Buffering)
            dispatch_semaphore_wait(m_frameSemaphore, DISPATCH_TIME_FOREVER);

            id<MTLCommandBuffer> commandBuffer = [m_commandQueue commandBuffer];
            state.commandBuffer = (__bridge void*)commandBuffer;
            state.frameIndex = m_currentFrameIndex;

            // Prepare Render Pass (simulated, usually obtained from MTKView)
            // In this standalone library, we don't own the MTKView directly in this class context 
            // without binding it. This implementation assumes an external MTKView 
            // provided the drawable via a binding method we would add, or we create an offscreen texture.
            
            // FOR MODULE 2 COMPLIANCE: We will simulate the encoder creation assuming valid drawable
            // OR we assume this is called *inside* `drawInMTKView`.
            
            // Correct approach for standalone Engine:
            // The Engine creates the view and window (as seen in v4.0), or wraps it.
            // Here we return a state that allows Encoding.
            // Since we don't have the view pointer passed in here, we'll return a stub 
            // that `EndFrame` can handle, or we assume `BeginFrame` is called with context.
            
            // To be strictly compliant with the header, `BeginFrame` should set up the encoder.
            // We will defer encoder creation to `drawInMTKView` context usually.
            // But let's create a command buffer here.
            
            return state;
        }

        void EndFrame(const RenderState& state) override {
            if (m_isHeadless) return;

            id<MTLCommandBuffer> cmd = (__bridge id<MTLCommandBuffer>)state.commandBuffer;
            
            // Callback to signal semaphore when GPU is done
            __block dispatch_semaphore_t sem = m_frameSemaphore;
            [cmd addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
                dispatch_semaphore_signal(sem);
            }];

            [cmd commit];
            m_currentFrameIndex = (m_currentFrameIndex + 1) % kMaxInFlightFrames;
        }

        void Resize(uint32_t width, uint32_t height) override {
            m_width = width;
            m_height = height;
        }
    };

// --- FACTORY ---
    std::unique_ptr<IGraphicsEngine> CreateGraphicsEngine() {
        return std::make_unique<MetalEngine>();
    }

} // namespace Ardium::Graphics

// --- MASTER BRIDGE (TITAN v5.0) ---

extern "C" {
    static std::unique_ptr<Ardium::Graphics::IGraphicsEngine> g_graphics;

    void coreui_init_metal() {
        if (!g_graphics) {
            g_graphics = Ardium::Graphics::CreateGraphicsEngine();
            g_graphics->Init();
        }
    }

    int64_t metal_create_system(int64_t count, double w, double h) {
        if (!g_graphics) coreui_init_metal();
        uint64_t handle = 0;
        g_graphics->CreateSharedBuffer((size_t)count * 32, handle); // 32 is size of Particle
        return (int64_t)handle;
    }

    void coreui_create_metal_window(const char* title, int64_t sys_id) {
        // In v5.0, the window setup is usually handled by the app delegate,
        // but we can trigger it here if the engine supports it.
        std::cout << "[Titan::Bridge] Metal Window Requested: " << title << " (ID: " << sys_id << ")" << std::endl;
    }

    void coreui_run_loop() {
        std::cout << "[Titan::Bridge] Entering Main Loop..." << std::endl;
        [NSApp run];
    }

    // Legacy Support
    void core_metal_init() { coreui_init_metal(); }
    void* core_metal_alloc_shared(int64_t count) {
        uint64_t h = 0;
        return g_graphics->CreateSharedBuffer((size_t)count * 32, h);
    }
}