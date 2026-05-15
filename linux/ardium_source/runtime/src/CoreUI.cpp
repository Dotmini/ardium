#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <iostream>
#include <thread>
#include <chrono>

extern "C" void* create_native_window(int width, int height, const char* title);
extern "C" void process_events();

// Internal state
static MTL::Device* g_device = nullptr;
static CA::MetalLayer* g_layer = nullptr;

extern "C" {

    void CoreUI_Init() {
        NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
        
        g_device = MTL::CreateSystemDefaultDevice();
        if (g_device) {
            std::cout << "CoreUI: Metal Device Initialized (" << g_device->name()->utf8String() << ")" << std::endl;
        } else {
            std::cerr << "CoreUI: Failed to create Metal Device." << std::endl;
        }

        pPool->release();
    }

    void CoreUI_Window(const char* title, int x, int y, int w, int h) {
        if (!g_device) {
            std::cerr << "CoreUI: Device not initialized. Call agui() first." << std::endl;
            return;
        }
        
        // Ignoring x/y for now in this simple shim
        void* raw_layer = create_native_window(w, h, title);
        g_layer = (CA::MetalLayer*)raw_layer;
        g_layer->setDevice(g_device);
        g_layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
        
        std::cout << "CoreUI: Window '" << title << "' created." << std::endl;
    }

    void CoreUI_Background() {
        // Placeholder: Set clear color or similar
        std::cout << "CoreUI: Background set (Stub)." << std::endl;
    }

    void CoreUI_Text(const char* content, int size, int bold) {
        std::cout << "CoreUI: Text '" << content << "' (Size: " << size << ")" << std::endl;
    }

    void CoreUI_Button(const char* label, long callback) {
        std::cout << "CoreUI: Button '" << label << "' registered." << std::endl;
    }

    void CoreUI_Image(const char* path) {
        std::cout << "CoreUI: Image '" << path << "' added." << std::endl;
    }

    long CoreUI_TextField(const char* placeholder, int is_secure) {
        std::cout << "CoreUI: TextField '" << placeholder << "' added." << std::endl;
        return 1001; // Mock Handle
    }

    const char* CoreUI_GetInputValue(long handle) {
        // Return dummy text for testing
        return "admin"; 
    }

    void CoreUI_Run() {
        std::cout << "CoreUI: Starting Run Loop..." << std::endl;
        // Simple event loop
        for(int i=0; i<500; i++) {
            process_events();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
        std::cout << "CoreUI: Run Loop ended." << std::endl;
    }

    // Deprecated / Alias
    void gpu_init() {
        CoreUI_Init();
    }
}
