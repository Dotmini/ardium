#pragma once

#include "ArdiumOS.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 5: TITAN EXECUTION ENGINE
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  Manages the lifecycle of Ardium logic modules. 
 *  Supports High-Speed JIT execution and "Zero-Downtime" Hot-Swapping 
 *  via Dynamic Library (.dylib) injection.
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    /**
     * @class DynamicModule
     * @brief A handle to a loaded binary logic unit.
     */
    class DynamicModule {
    private:
        void* m_handle = nullptr;
        std::string m_path;

    public:
        DynamicModule(const std::string& path);
        ~DynamicModule();

        bool load();
        void unload();
        void* get_symbol(const std::string& name);
        
        bool is_loaded() const { return m_handle != nullptr; }
        const std::string& path() const { return m_path; }
    };

    /**
     * @class ExecutionEngine
     * @brief Orchestrates module loading and hot-swapping.
     */
    class ExecutionEngine {
    public:
        virtual ~ExecutionEngine() = default;

        /**
         * @brief Initialize the execution environment.
         */
        virtual void Init() = 0;

        /**
         * @brief Load a module and make its logic active.
         * @param path Path to the .dylib or .bc file.
         */
        virtual bool SwapModule(const std::string& path) = 0;

        /**
         * @brief Invoke a function by name from the active module.
         * @param name Symbol name.
         * @param arg Generic argument.
         */
        virtual int64_t Invoke(const std::string& name, int64_t arg) = 0;

        /**
         * @brief Check if a symbol exists in the current execution context.
         */
        virtual bool HasSymbol(const std::string& name) = 0;
    };

    /**
     * @brief Factory to create the Execution Engine instance.
     */
    std::unique_ptr<ExecutionEngine> CreateExecutionEngine();

} // namespace Titan
} // namespace Ardium
