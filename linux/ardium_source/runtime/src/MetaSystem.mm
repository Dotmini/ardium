#include "../include/TitanObject.h"
#include <iostream>
#include <mutex>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 2: META SYSTEM IMPLEMENTATION
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  Global Registry for all Titan Classes. 
 *  Provides the entry point for the Hyper-Dispatch Engine.
 * ============================================================================
 */

namespace Ardium::Titan {

    class MetaRegistry {
        std::unordered_map<std::string, std::unique_ptr<ClassMetadata>> m_classes;
        std::mutex m_mtx;

    public:
        static MetaRegistry& instance() {
            static MetaRegistry reg;
            return reg;
        }

        void register_class(std::unique_ptr<ClassMetadata> cls) {
            std::lock_guard<std::mutex> lock(m_mtx);
            std::cout << "[Titan::Meta] Registering Class: " << cls->name << std::endl;
            m_classes[cls->name] = std::move(cls);
        }

        ClassMetadata* get_class(const std::string& name) {
            std::lock_guard<std::mutex> lock(m_mtx);
            auto it = m_classes.find(name);
            return (it != m_classes.end()) ? it->second.get() : nullptr;
        }
    };

    // --- Hyper-Dispatch Engine ---
    
    /**
     * @brief The global "Message Send" for Titan.
     * Equivalent to objc_msgSend but optimized for Titan's CMOS.
     */
    extern "C" Object* titan_dispatch(Object* receiver, const char* method_name, const std::vector<Object*>& args) {
        if (!receiver) {
            std::cerr << "[Titan::Dispatch] Error: nil receiver for method '" << method_name << "'" << std::endl;
            return nullptr;
        }

        ClassMetadata* cls = receiver->get_class();
        MethodFunc func = cls->lookup_method(method_name);

        if (func) {
            return func(receiver, args);
        }

        std::cerr << "[Titan::Dispatch] Fatal: Method '" << method_name 
                  << "' not found in class '" << cls->name << "'" << std::endl;
        return nullptr;
    }

} // namespace Ardium::Titan
