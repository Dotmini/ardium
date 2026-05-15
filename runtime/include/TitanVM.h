#pragma once
#include "TitanObject.h"

namespace Ardium::Titan {

    /**
     * @class VirtualMachine
     * @brief The core execution engine for Titan.
     */
    class VirtualMachine {
    public:
        static void Boot();
        static void Shutdown();
        
        static void LoadModule(const std::string& path);
    };

}
