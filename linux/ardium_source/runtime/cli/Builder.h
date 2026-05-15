#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <memory>

namespace Ardium::CLI {

    class Builder {
    public:
        static bool RunCommand(const std::string& cmd) {
            // std::cout << "[DEBUG] Executing: " << cmd << std::endl;
            int result = std::system(cmd.c_str());
            return result == 0;
        }

        static std::string ExecCapture(const char* cmd) {
            std::array<char, 128> buffer;
            std::string result;
            std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
            if (!pipe) return "Failed";
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            return result;
        }

        static void CompileApp(const std::string& sourceFile, const std::string& outputName, bool runAfter) {
            // Step 1: Transpile Ardium (.ar) to C++ (.cpp)
            // Ideally, we use the Titan Compiler logic here to generate the C++ shim.
            // For v5.0 CLI demo, we will generate a C++ stub that loads the script into the Titan VM.
            
            std::cout << "\027[1;34m[===       ]\027[0m Generating Native Shim..." << std::endl;
            
            std::string cppFile = outputName + "_shim.cpp";
            FILE* f = fopen(cppFile.c_str(), "w");
            if(f) {
                fprintf(f, "#include \"runtime/include/TitanVM.h\"\n");
                fprintf(f, "#include \"runtime/include/TitanExecution.h\"\n");
                fprintf(f, "#include <iostream>\n\n");
                fprintf(f, "int main() {\n");
                fprintf(f, "    Ardium::Titan::VirtualMachine::Boot();\n");
                // In a real transpiler, we would embed the bytecode here.
                // For now, we load the script dynamically or assume it's just a runtime harness.
                fprintf(f, "    std::cout << \"[Shim] Running Script: %s\" << std::endl;\n", sourceFile.c_str());
                fprintf(f, "    // Here we would call the JIT or Compiler\n");
                fprintf(f, "    Ardium::Titan::VirtualMachine::Shutdown();\n");
                fprintf(f, "    return 0;\n");
                fprintf(f, "}\n");
                fclose(f);
            }

            // Step 2: Compile C++ Stub with Titan Runtime
            std::cout << "\027[1;34m[======    ]\027[0m Compiling with Clang..." << std::endl;
            
            std::string compileCmd = "clang++ -std=c++20 -O3 -fobjc-arc ";
            compileCmd += cppFile + " ";
            compileCmd += "runtime/build/TitanVM.o "; 
            compileCmd += "runtime/build/MemoryController.o "; 
            compileCmd += "runtime/build/ArdiumOS_Mac.o "; 
            // Add other modules or link libTitan.a if available
            compileCmd += "-L. -lTitan "; // Link the big lib
            compileCmd += "-framework Cocoa -framework Metal -framework MetalKit -framework Vision -framework CoreML -framework CoreVideo -framework Foundation ";
            compileCmd += "-o " + outputName;

            if (RunCommand(compileCmd)) {
                std::cout << "\027[1;32m[==========] Build Successful!\027[0m" << std::endl;
                if (runAfter) {
                    std::cout << ">>> Running " << outputName << "..." << std::endl;
                    RunCommand("./" + outputName);
                }
            } else {
                std::cerr << "\027[1;31m[ERROR] Compilation Failed.\027[0m" << std::endl;
            }
        }
    };

} // namespace Ardium::CLI
