#pragma once

#include "ArdiumOS.h"
#include "ArdiumGraphics.h"
#include <string>
#include <vector>
#include <memory>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 4: TITAN COREUI (LAYOUT ENGINE)
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  High-level Declarative UI Engine.
 *  Handles Flex-box style layouts (VClass, HClass) and UI Event Bubbling.
 *  Built on top of the Module 2 Metal Engine.
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    enum class LayoutType { Vertical, Horizontal, Stack };

    /**
     * @struct UIStyle
     * @brief Visual properties for a UI component.
     */
    struct UIStyle {
        float width = 0;
        float height = 0;
        float padding = 0;
        float margin = 0;
        uint32_t backgroundColor = 0x00000000;
    };

    /**
     * @class UIComponent
     * @brief Base class for all UI elements.
     */
    class UIComponent {
    protected:
        std::string m_id;
        UIStyle m_style;
        std::vector<std::shared_ptr<UIComponent>> m_children;
        bool m_isDirty = true;

    public:
        UIComponent(const std::string& id) : m_id(id) {}
        virtual ~UIComponent() = default;

        void add_child(std::shared_ptr<UIComponent> child) {
            m_children.push_back(child);
            m_isDirty = true;
        }

        virtual void update_layout(float parentWidth, float parentHeight) = 0;
        virtual void render(Graphics::IGraphicsEngine* engine) = 0;

        const std::string& id() const { return m_id; }
        UIStyle& style() { return m_style; }
    };

    /**
     * @class Container
     * @brief A layout container (Flexbox logic).
     */
    class Container : public UIComponent {
        LayoutType m_type;
    public:
        Container(const std::string& id, LayoutType type) : UIComponent(id), m_type(type) {}

        void update_layout(float parentWidth, float parentHeight) override;
        void render(Graphics::IGraphicsEngine* engine) override;
    };

    /**
     * @class UIButton
     * @brief A clickable interactive element.
     */
    class UIButton : public UIComponent {
    public:
        UIButton(const std::string& id) : UIComponent(id) {}
        void update_layout(float parentWidth, float parentHeight) override;
        void render(Graphics::IGraphicsEngine* engine) override;
    };

} // namespace Titan
} // namespace Ardium
