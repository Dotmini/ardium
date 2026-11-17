#include "../include/ArcCompiler.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "error: no input file provided\nusage: arc run <file.ar>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "error: could not open file '" << argv[1] << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    Ardium::Arc::Compiler::Program prog;
    if (Ardium::Arc::Compiler::Compiler::Compile(source, prog)) {
        Ardium::Arc::Compiler::Interpreter::Run(prog);
        return 0;
    } else {
        std::cerr << "error: compilation failed\n";
        return 1;
    }
}