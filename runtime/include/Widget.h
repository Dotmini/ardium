#pragma once

#include "Reactive.h"
#include <string>
#include <iostream>

namespace Ardium::Titan {

    /**
     * @class Widget
     * @brief Abstract base class for UI components.
     */
    class Widget {
    public:
        virtual ~Widget() = default;
        virtual void Render() = 0;
    };

    /**
     * @class Label
     * @brief A text component that can bind to data sources.
     */
    class Label : public Widget {
    private:
        std::string m_text;
        std::string m_prefix;

    public:
        Label(const std::string& prefix = "") : m_prefix(prefix) {}

        void Render() override {
            std::cout << "[UI] " << m_prefix << m_text << std::endl;
        }

        /**
         * @brief Binds the label to an Integer state.
         */
        void Bind(State<int>& state) {
            // Set initial value
            m_text = std::to_string(state.Get());
            
            // Auto-update on state change
            state.Listen([this](const int& val) {
                this->m_text = std::to_string(val);
                this->Render(); // Trigger repaint
            });
        }

        /**
         * @brief Binds the label to a String state.
         */
        void Bind(State<std::string>& state) {
            m_text = state.Get();
            state.Listen([this](const std::string& val) {
                this->m_text = val;
                this->Render();
            });
        }
    };

} // namespace Ardium::Titan
