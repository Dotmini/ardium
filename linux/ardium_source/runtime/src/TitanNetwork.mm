#include "../include/TitanNetwork.h"
#include "../include/ArdiumOS.h"
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

// Logging extern
namespace Ardium::Memory { extern void Log(const char* fmt, ...); }

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 5: NETWORK IMPLEMENTATION (POSIX)
 * ============================================================================
 *  Architect: Major
 *  Platform:  macOS / POSIX
 * ============================================================================
 */

namespace Ardium::Titan {

    // --- PACKET STRUCTURE ---
    // Simple TLV (Type-Length-Value) header for stream processing
    struct PacketHeader {
        uint32_t magic; // 0x41524449 "ARDI"
        uint32_t type;  // 0=Heartbeat, 1=BusMessage
        uint32_t size;  // Payload size
    };

    class PosixNetworkEngine : public NetworkEngine {
        int m_server_socket = -1;
        std::vector<int> m_clients;
        std::mutex m_client_mtx;
        std::atomic<bool> m_running{false};
        std::unique_ptr<HAL::IThread> m_listen_thread;
        std::vector<std::unique_ptr<HAL::IThread>> m_client_threads;

    public:
        PosixNetworkEngine() {
            m_listen_thread = HAL::OSFactory::CreateThread();
        }

        ~PosixNetworkEngine() {
            Shutdown();
        }

        void Init() override {
            Ardium::Memory::Log("[Titan::Network] Initializing POSIX Networking...");
            // Bridge to Titan Bus for incoming network messages
            // UnifiedBus::Instance().subscribe("net.out", ...); 
        }

        bool Listen(int port) override {
            m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (m_server_socket < 0) {
                Ardium::Memory::Log("[Titan::Network] Failed to create socket.");
                return false;
            }

            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            // Allow reuse
            int opt = 1;
            setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            if (bind(m_server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                Ardium::Memory::Log("[Titan::Network] Bind failed.");
                return false;
            }

            if (listen(m_server_socket, 5) < 0) {
                Ardium::Memory::Log("[Titan::Network] Listen failed.");
                return false;
            }

            m_running = true;
            m_listen_thread->spawn([this]() { this->AcceptLoop(); }, HAL::IThread::Priority::Normal, "TitanNetListener");
            
            Ardium::Memory::Log("[Titan::Network] Listening on port %d", port);
            return true;
        }

        bool Connect(const std::string& ip, int port) override {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return false;

            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                Ardium::Memory::Log("[Titan::Network] Connection to %s:%d failed.", ip.c_str(), port);
                close(sock);
                return false;
            }

            std::lock_guard<std::mutex> lock(m_client_mtx);
            m_clients.push_back(sock);
            Ardium::Memory::Log("[Titan::Network] Connected to %s:%d", ip.c_str(), port);
            
            // Spawn reader thread for this socket
            // In v5.0, we just push to list. A real reactor/epoll system would be better for scale.
            
            return true;
        }

        void Broadcast(const Message& msg) override {
            // Serialization (Basic)
            // Format: [TopicLen:4][TopicBytes][Type:4][Data:8]
            std::string topic = msg.topic;
            uint32_t tlen = (uint32_t)topic.length();
            int64_t dataVal = 0;
            
            // Visitor extraction
            if (std::holds_alternative<int64_t>(msg.data)) dataVal = std::get<int64_t>(msg.data);
            // else if double... ignore for demo simplicity
            
            std::vector<uint8_t> buffer;
            buffer.resize(sizeof(tlen) + tlen + sizeof(int64_t));
            
            uint8_t* ptr = buffer.data();
            memcpy(ptr, &tlen, 4); ptr += 4;
            memcpy(ptr, topic.c_str(), tlen); ptr += tlen;
            memcpy(ptr, &dataVal, 8);

            std::lock_guard<std::mutex> lock(m_client_mtx);
            for (int sock : m_clients) {
                send(sock, buffer.data(), buffer.size(), 0);
            }
        }

        void Shutdown() override {
            m_running = false;
            if (m_server_socket >= 0) close(m_server_socket);
            
            std::lock_guard<std::mutex> lock(m_client_mtx);
            for (int sock : m_clients) close(sock);
            m_clients.clear();
            
            m_listen_thread->join();
            Ardium::Memory::Log("[Titan::Network] Subsystem shutdown.");
        }

    private:
        void AcceptLoop() {
            while (m_running) {
                sockaddr_in cli_addr;
                socklen_t clilen = sizeof(cli_addr);
                int newsock = accept(m_server_socket, (struct sockaddr*)&cli_addr, &clilen);
                
                if (!m_running) break;
                if (newsock < 0) continue;

                {
                    std::lock_guard<std::mutex> lock(m_client_mtx);
                    m_clients.push_back(newsock);
                }
                Ardium::Memory::Log("[Titan::Network] Accepted client.");
            }
        }
    };

    std::unique_ptr<NetworkEngine> CreateNetworkEngine() {
        return std::make_unique<PosixNetworkEngine>();
    }

} // namespace Ardium::Titan

// --- C-API BRIDGE ---

extern "C" {
    static std::unique_ptr<Ardium::Titan::NetworkEngine> g_net_engine;

    void titan_net_init() {
        g_net_engine = Ardium::Titan::CreateNetworkEngine();
        g_net_engine->Init();
    }

    int64_t titan_net_listen(int64_t port) {
        if (g_net_engine) return g_net_engine->Listen((int)port) ? 1 : 0;
        return 0;
    }

    int64_t titan_net_connect(const char* ip, int64_t port) {
        if (g_net_engine) return g_net_engine->Connect(ip, (int)port) ? 1 : 0;
        return 0;
    }
}
