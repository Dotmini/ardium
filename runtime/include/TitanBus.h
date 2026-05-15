#pragma once

#include "ArdiumOS.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <variant>
#include <queue>
#include <mutex>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 3: TITAN UNIFIED BUS (TUB)
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  The central nervous system of the Titan Runtime. 
 *  Provides a high-speed, thread-safe message-passing interface for 
 *  inter-module communication (e.g., Physics -> UI, AI -> Kernel).
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    /**
     * @brief Supported Message Payload Types
     */
    using Payload = std::variant<int64_t, double, std::string, void*>;

    /**
     * @struct Message
     * @brief Unified message packet.
     */
    struct Message {
        std::string topic;
        Payload data;
        uint64_t timestamp;
        int64_t sender_id;
    };

    /**
     * @typedef MessageHandler
     * @brief Callback type for message subscribers.
     */
    using MessageHandler = std::function<void(const Message&)>;

    /**
     * @class UnifiedBus
     * @brief Singleton message distributor.
     */
    class UnifiedBus {
    private:
        std::map<std::string, std::vector<MessageHandler>> m_subscribers;
        std::queue<Message> m_async_queue;
        std::mutex m_bus_mtx;
        std::unique_ptr<HAL::IMutex> m_queue_mtx;
        std::unique_ptr<HAL::IThread> m_dispatch_thread;
        std::atomic<bool> m_running{false};

        UnifiedBus(); // Private constructor

    public:
        static UnifiedBus& Instance();

        /**
         * @brief Subscribe to a specific topic.
         */
        void subscribe(const std::string& topic, MessageHandler handler);

        /**
         * @brief Publish a message synchronously (immediate dispatch).
         */
        void publish_sync(const Message& msg);

        /**
         * @brief Publish a message asynchronously (queued).
         */
        void publish_async(const Message& msg);

        /**
         * @brief Start the background dispatch thread for async messages.
         */
        void start();

        /**
         * @brief Stop the bus.
         */
        void stop();

    private:
        void dispatch_loop();
    };

} // namespace Titan
} // namespace Ardium
