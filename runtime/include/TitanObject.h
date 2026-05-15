#pragma once

#include "ArdiumOS.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 2: CORE META-OBJECT SYSTEM (CMOS)
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  Defines the root hierarchy for all high-level Ardium objects.
 *  Implements "Titan ARC" (Advanced Reference Counting) and the 
 *  Dynamic Dispatch VTable.
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    class ClassMetadata;

    /**
     * @class Object
     * @brief Root class for the Titan Runtime.
     */
    class Object {
    private:
        mutable std::atomic<int32_t> m_ref_count{1};
        ClassMetadata* m_class_ptr;

    public:
        Object(ClassMetadata* cls) : m_class_ptr(cls) {}
        virtual ~Object() = default;

        // --- TITAN ARC (Advanced Reference Counting) ---
        void retain() const { m_ref_count.fetch_add(1, std::memory_order_relaxed); }
        void release() const {
            if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete this;
            }
        }

        int32_t ref_count() const { return m_ref_count.load(); }
        ClassMetadata* get_class() const { return m_class_ptr; }
    };

    /**
     * @typedef MethodFunc
     * @brief Signature for Titan dynamic methods.
     * Takes (this, args_vector) and returns an Object*.
     */
    typedef Object* (*MethodFunc)(Object*, const std::vector<Object*>&);

    /**
     * @class ClassMetadata
     * @brief Reflection and Dispatch information for a Titan Class.
     */
    class ClassMetadata {
    public:
        std::string name;
        ClassMetadata* super_class;
        std::unordered_map<std::string, MethodFunc> vtable;

        ClassMetadata(const std::string& n, ClassMetadata* parent = nullptr) 
            : name(n), super_class(parent) {}

        /**
         * @brief Fast method lookup with inheritance traversal.
         */
        MethodFunc lookup_method(const std::string& method_name) {
            auto it = vtable.find(method_name);
            if (it != vtable.end()) return it->second;
            if (super_class) return super_class->lookup_method(method_name);
            return nullptr;
        }
    };

    /**
     * @brief Smart Pointer for Titan Objects (TitanRef)
     */
    template<typename T>
    class TitanRef {
        T* m_ptr = nullptr;
    public:
        TitanRef() = default;
        TitanRef(T* p) : m_ptr(p) { if (m_ptr) m_ptr->retain(); }
        TitanRef(const TitanRef& other) : TitanRef(other.m_ptr) {}
        ~TitanRef() { if (m_ptr) m_ptr->release(); }

        T* operator->() const { return m_ptr; }
        T* get() const { return m_ptr; }
        
        bool is_null() const { return m_ptr == nullptr; }
    };

} // namespace Titan
} // namespace Ardium
