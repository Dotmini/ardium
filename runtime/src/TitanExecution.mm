#include "../include/TitanExecution.h"
#include <iostream>
#include <dlfcn.h>
#include <mutex>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 5: EXECUTION IMPLEMENTATION
 * ============================================================================
 *  Architect: Major
 *  Platform:  macOS (dlopen / dynamic-link)
 * ============================================================================
 */

namespace Ardium::Titan {

    // --- DynamicModule Implementation ---

    DynamicModule::DynamicModule(const std::string& path) : m_path(path) {}

    DynamicModule::~DynamicModule() {
        unload();
    }

    bool DynamicModule::load() {
        if (m_handle) unload();
        
        // RTLD_NOW: Perform all relocations now.
        // RTLD_GLOBAL: Symbols available to other modules (needed for interop).
        m_handle = dlopen(m_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        
        if (!m_handle) {
            std::cerr << "[Titan::Execution] Load Error: " << dlerror() << std::endl;
            return false;
        }
        
        std::cout << "[Titan::Execution] Module Loaded: " << m_path << std::endl;
        return true;
    }

    void DynamicModule::unload() {
        if (m_handle) {
            dlclose(m_handle);
            m_handle = nullptr;
            std::cout << "[Titan::Execution] Module Unloaded." << std::endl;
        }
    }

    void* DynamicModule::get_symbol(const std::string& name) {
        if (!m_handle) return nullptr;
        void* sym = dlsym(m_handle, name.c_str());
        if (!sym) {
            // Some compilers prepend an underscore on macOS
            std::string underscore_name = "_" + name;
            sym = dlsym(m_handle, underscore_name.c_str());
        }
        return sym;
    }

    // --- TitanExecutionEngine Implementation ---

    class TitanExecutionEngine : public ExecutionEngine {
        std::unique_ptr<DynamicModule> m_active_module;
        std::mutex m_mtx;
        std::unordered_map<std::string, void*> m_symbol_cache;

    public:
        void Init() override {
            std::cout << "[Titan::Execution] JIT Context Initialized." << std::endl;
        }

        bool SwapModule(const std::string& path) override {
            std::lock_guard<std::mutex> lock(m_mtx);
            
            auto new_module = std::make_unique<DynamicModule>(path);
            if (new_module->load()) {
                m_active_module = std::move(new_module);
                m_symbol_cache.clear(); // Invalidate cache on swap
                std::cout << "[Titan::Execution] --- HOT SWAP COMPLETE ---" << std::endl;
                return true;
            }
            return false;
        }

        int64_t Invoke(const std::string& name, int64_t arg) override {
            void* sym = nullptr;
            
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                if (m_symbol_cache.count(name)) {
                    sym = m_symbol_cache[name];
                } else if (m_active_module) {
                    sym = m_active_module->get_symbol(name);
                    if (sym) m_symbol_cache[name] = sym;
                }
            }

            if (sym) {
                // Function pointer signature: int64_t func(int64_t)
                typedef int64_t (*ArdiumFunc)(int64_t);
                ArdiumFunc func = (ArdiumFunc)sym;
                return func(arg);
            }

            return -1; // Symbol not found
        }

        bool HasSymbol(const std::string& name) override {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_symbol_cache.count(name)) return true;
            return m_active_module && m_active_module->get_symbol(name) != nullptr;
        }
    };

    std::unique_ptr<ExecutionEngine> CreateExecutionEngine() {
        return std::make_unique<TitanExecutionEngine>();
    }

} // namespace Ardium::Titan

// --- C-API BRIDGE ---

extern "C" {
    static std::unique_ptr<Ardium::Titan::ExecutionEngine> g_engine;

    void titan_exec_init() {
        g_engine = Ardium::Titan::CreateExecutionEngine();
        g_engine->Init();
    }

    int64_t titan_exec_swap(const char* path) {
        if (g_engine) return g_engine->SwapModule(path) ? 1 : 0;
        return 0;
    }

    int64_t titan_exec_invoke(const char* name, int64_t arg) {
        if (g_engine) return g_engine->Invoke(name, arg);
        return -1;
    }
}
