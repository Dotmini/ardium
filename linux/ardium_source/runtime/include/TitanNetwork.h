#pragma once

#include "ArdiumOS.h"
#include "TitanBus.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 5: TITAN NETWORKING & CLUSTER CONTROL
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  Provides high-performance TCP/UDP networking.
 *  Integrates with Titan Unified Bus to allow "Network-Transparent" messaging.
 *  Enables Ardium instances to form a computational cluster.
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    enum class NetworkMode {
        Standalone,
        ClusterNode, // Client
        ClusterHead  // Server
    };

    /**
     * @class NetworkEngine
     * @brief Manages socket connections and orchestrates cluster data flow.
     */
    class NetworkEngine {
    public:
        virtual ~NetworkEngine() = default;

        /**
         * @brief Initialize the network subsystem.
         */
        virtual void Init() = 0;

        /**
         * @brief Start listening on a specific port (Server Mode).
         */
        virtual bool Listen(int port) = 0;

        /**
         * @brief Connect to a remote Titan Node.
         */
        virtual bool Connect(const std::string& ip, int port) = 0;

        /**
         * @brief Send a raw message to all connected nodes.
         */
        virtual void Broadcast(const Message& msg) = 0;

        /**
         * @brief Shutdown network threads and sockets.
         */
        virtual void Shutdown() = 0;
    };

    /**
     * @brief Factory to create the Network Engine.
     */
    std::unique_ptr<NetworkEngine> CreateNetworkEngine();

} // namespace Titan
} // namespace Ardium
