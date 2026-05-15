#include "../include/TitanUI.h"
#include <iostream>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 4: COREUI IMPLEMENTATION
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  Recursive Layout and Rendering logic.
 *  Uses the Metal Graphics Engine for drawing primitives.
 * ============================================================================
 */

namespace Ardium::Titan {

    // --- Container Implementation ---

    void Container::update_layout(float parentWidth, float parentHeight) {
        float currentX = 0;
        float currentY = 0;

        // Simplified Flex-box logic
        for (auto& child : m_children) {
            child->update_layout(m_style.width > 0 ? m_style.width : parentWidth, 
                                 m_style.height > 0 ? m_style.height : parentHeight);
            
            if (m_type == LayoutType::Vertical) {
                currentY += child->style().height + child->style().margin;
            } else if (m_type == LayoutType::Horizontal) {
                currentX += child->style().width + child->style().margin;
            }
        }
        
        if (m_isDirty) {
            std::cout << "[Titan::UI] Layout Updated for Container: " << m_id << std::endl;
            m_isDirty = false;
        }
    }

    void Container::render(Graphics::IGraphicsEngine* engine) {
        // Draw background if color set
        if (m_style.backgroundColor > 0) {
            // engine->DrawRect(...) - Stub for Module 2 link
        }

        for (auto& child : m_children) {
            child->render(engine);
        }
    }

    // --- UIButton Implementation ---

    void UIButton::update_layout(float parentWidth, float parentHeight) {
        // Buttons typically have intrinsic size or fill
        if (m_style.width == 0) m_style.width = 100;
        if (m_style.height == 0) m_style.height = 40;
    }

    void UIButton::render(Graphics::IGraphicsEngine* engine) {
        // std::cout << "[Titan::UI] Rendering Button: " << m_id << std::endl;
    }

} // namespace Ardium::Titan

// --- C-API BRIDGE ---

extern "C" {
    static std::vector<std::shared_ptr<Ardium::Titan::Container>> g_ui_stack;

    void titan_ui_begin_container(int32_t type) {
        std::string id = "container_" + std::to_string(g_ui_stack.size());
        auto container = std::make_shared<Ardium::Titan::Container>(id, (Ardium::Titan::LayoutType)type);
        
        if (!g_ui_stack.empty()) {
            g_ui_stack.back()->add_child(container);
        }
        
        g_ui_stack.push_back(container);
        std::cout << "[Titan::UI] Begin Container Type: " << type << std::endl;
    }

    void titan_ui_end_container() {
        if (!g_ui_stack.empty()) {
            auto finished = g_ui_stack.back();
            g_ui_stack.pop_back();
            
            if (g_ui_stack.empty()) {
                // Root container finished, trigger layout
                finished->update_layout(1024, 768); // Assuming some default screen size
                std::cout << "[Titan::UI] Root UI Layout Finalized." << std::endl;
            }
        }
    }

    void titan_ui_create_button(const char* label) {
        auto btn = std::make_shared<Ardium::Titan::UIButton>(label);
        if (!g_ui_stack.empty()) {
            g_ui_stack.back()->add_child(btn);
        }
        std::cout << "[Titan::UI] Button Created: " << label << std::endl;
    }

    void* titan_ui_create_container(const char* id, int32_t type) {
        auto container = std::make_shared<Ardium::Titan::Container>(id, (Ardium::Titan::LayoutType)type);
        return new std::shared_ptr<Ardium::Titan::UIComponent>(container);
    }

    void titan_ui_add_child(void* parent_ref, void* child_ref) {
        auto parent = *(std::shared_ptr<Ardium::Titan::UIComponent>*)parent_ref;
        auto child = *(std::shared_ptr<Ardium::Titan::UIComponent>*)child_ref;
        parent->add_child(child);
    }
}
