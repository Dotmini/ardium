#pragma once

#include <vector>
#include <functional>
#include <mutex>
#include <iostream>

namespace Ardium::Titan {

    /**
     * @class State
     * @brief A thread-safe reactive variable wrapper.
     * Notifies listeners whenever the value changes.
     */
    template <typename T>
    class State {
    private:
        T m_value;
        std::vector<std::function<void(const T&)>> m_listeners;
        mutable std::mutex m_mtx;

    public:
        State(T initialValue) : m_value(initialValue) {}

        /**
         * @brief Update the value and trigger all listeners.
         * Thread-safe.
         */
        void Set(const T& newValue) {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_value != newValue) {
                m_value = newValue;
                Notify();
            }
        }

        /**
         * @brief Get the current value.
         * Thread-safe.
         */
        T Get() const {
            std::lock_guard<std::mutex> lock(m_mtx);
            return m_value;
        }

        /**
         * @brief Register a callback to be invoked on change.
         */
        void Listen(std::function<void(const T&)> callback) {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_listeners.push_back(callback);
        }

    private:
        void Notify() {
            // In a complex system, we might copy listeners to avoid deadlock 
            // if a listener calls Set() recursively. 
            // For now, we iterate directly assuming well-behaved listeners.
            for (const auto& callback : m_listeners) {
                callback(m_value);
            }
        }
    };

} // namespace Ardium::Titan
