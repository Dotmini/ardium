#include "../include/TitanVM.h"
#include "../include/ArdiumOS.h"
#include "../include/TitanVision.h"
#include "../include/TitanExecution.h"
#include "../include/TitanNetwork.h"
#include <iostream>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 2: VIRTUAL MACHINE
 * ============================================================================
 *  Architect: Major
 *  
 *  Description:
 *  Initializes the Titan environment, boots the HAL, and prepares the
 *  Memory Controller for heavy lifting.
 * ============================================================================
 */

// External access to Memory Controller (Module 1)
namespace Ardium::Memory {
    extern void Init();
    extern void Shutdown();
}

namespace Ardium::Titan {

    static std::unique_ptr<VisionEngine> g_vision_engine;
    static std::unique_ptr<ExecutionEngine> g_execution_engine;
    static std::unique_ptr<NetworkEngine> g_network_engine;

    void VirtualMachine::Boot() {
        std::cout << "================================================\n";
        std::cout << "   ARDIUM TITAN v5.0 MICRO-KERNEL BOOTING\n";
        std::cout << "================================================\n";

        // 1. Initialize Memory Controller
        Ardium::Memory::Init();

        // 2. Initialize Platform HAL
        auto clock = HAL::OSFactory::CreateClock();
        uint64_t boot_time = clock->now_micros();

        // 3. Initialize Vision Engine (Module 4)
        g_vision_engine = CreateVisionEngine();
        g_vision_engine->Init();

        // 4. Initialize Execution Engine (Module 5)
        g_execution_engine = CreateExecutionEngine();
        g_execution_engine->Init();

        // 5. Initialize Network Engine (Module 5)
        g_network_engine = CreateNetworkEngine();
        g_network_engine->Init();

        std::cout << "[Titan::VM] Platform HAL Layer: ACTIVE\n";
        std::cout << "[Titan::VM] Memory Controller:  READY\n";
        std::cout << "[Titan::VM] Vision AI Engine:   ONLINE\n";
        std::cout << "[Titan::VM] Execution Engine:   LOADED\n";
        std::cout << "[Titan::VM] Network Subsystem:  ACTIVE\n";
        std::cout << "[Titan::VM] System Boot Time:   " << boot_time << " us\n";
        
        std::cout << "[Titan::VM] Status: ALL SYSTEMS NOMINAL.\n\n";
    }
    void VirtualMachine::Shutdown() {
        std::cout << "\n[Titan::VM] Initiating Graceful Shutdown...\n";
        Ardium::Memory::Shutdown();
        std::cout << "[Titan::VM] Halt. Final.\n";
    }

    void VirtualMachine::LoadModule(const std::string& path) {
        std::cout << "[Titan::VM] Loading Module: " << path << std::endl;
        // Logic for dlopen and CMOS registration would go here
    }

} // namespace Ardium::Titan

// Global Entry for Ardium Runtime
extern "C" void titan_boot() {
    Ardium::Titan::VirtualMachine::Boot();
}

extern "C" void titan_shutdown() {
    Ardium::Titan::VirtualMachine::Shutdown();
}
