#include "TitanCompiler.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: TitanScript <file.ar>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Could not open file " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    std::cout << "--- [Titan] Compiling ---" << std::endl;
    
    Ardium::Titan::Compiler::Program prog;
    if (Ardium::Titan::Compiler::Compiler::Compile(source, prog)) {
        std::cout << "--- [Titan] Executing Bytecode ---" << std::endl;
        Ardium::Titan::Compiler::Interpreter::Run(prog);
        std::cout << "--- [Titan] Finished ---" << std::endl;
        return 0;
    } else {
        std::cerr << "Compilation Failed." << std::endl;
        return 1;
    }
}