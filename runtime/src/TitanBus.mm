#include "../include/TitanBus.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 3: BUS IMPLEMENTATION
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  Thread-safe implementation of the Titan Unified Bus.
 *  Uses HAL primitives for cross-platform stability.
 * ============================================================================
 */

namespace Ardium::Titan {

    UnifiedBus::UnifiedBus() {
        m_queue_mtx = HAL::OSFactory::CreateMutex();
        m_dispatch_thread = HAL::OSFactory::CreateThread();
    }

    UnifiedBus& UnifiedBus::Instance() {
        static UnifiedBus instance;
        return instance;
    }

    void UnifiedBus::subscribe(const std::string& topic, MessageHandler handler) {
        std::lock_guard<std::mutex> lock(m_bus_mtx);
        m_subscribers[topic].push_back(handler);
        // std::cout << "[Titan::Bus] New subscriber for topic: " << topic << std::endl;
    }

    void UnifiedBus::publish_sync(const Message& msg) {
        std::vector<MessageHandler> handlers;
        
        {
            std::lock_guard<std::mutex> lock(m_bus_mtx);
            if (m_subscribers.find(msg.topic) != m_subscribers.end()) {
                handlers = m_subscribers[msg.topic];
            }
        }

        for (auto& handler : handlers) {
            handler(msg);
        }
    }

    void UnifiedBus::publish_async(const Message& msg) {
        m_queue_mtx->lock();
        m_async_queue.push(msg);
        m_queue_mtx->unlock();
    }

    void UnifiedBus::start() {
        if (m_running) return;
        m_running = true;
        
        m_dispatch_thread->spawn([this]() {
            this->dispatch_loop();
        }, HAL::IThread::Priority::High, "TitanBusDispatcher");
        
        std::cout << "[Titan::Bus] Dispatcher Thread Started." << std::endl;
    }

    void UnifiedBus::stop() {
        m_running = false;
        // In a real HAL, we'd need a condition variable or signal to wake the thread
        // For this demo, join works.
        m_dispatch_thread->join();
        std::cout << "[Titan::Bus] Dispatcher Thread Stopped." << std::endl;
    }

    void UnifiedBus::dispatch_loop() {
        while (m_running) {
            Message msg;
            bool has_msg = false;

            m_queue_mtx->lock();
            if (!m_async_queue.empty()) {
                msg = m_async_queue.front();
                m_async_queue.pop();
                has_msg = true;
            }
            m_queue_mtx->unlock();

            if (has_msg) {
                publish_sync(msg);
            } else {
                // Yield CPU if queue is empty
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

} // namespace Ardium::Titan

// --- C-API BRIDGE ---

extern "C" {
    void titan_bus_publish(const char* topic, int64_t data) {
        Ardium::Titan::Message msg;
        msg.topic = topic;
        msg.data = data;
        msg.timestamp = 0; // Should get from HAL::Clock
        Ardium::Titan::UnifiedBus::Instance().publish_async(msg);
    }
}
