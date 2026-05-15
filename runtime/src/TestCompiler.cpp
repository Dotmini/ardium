#include "TitanCompiler.h"
#include <iostream>

int main() {
    std::string source = 
        "let G = 100;\n"
        "fn main() {\n"
        "  var x = 10;\n"
        "  GLOBAL(x) as Pointer;\n"
        "}";
        
    std::cout << "Input Code:\n" << source << "\n\n";
    std::cout << "AST LOG:\n";
    
    try {
        auto ast = Ardium::Titan::Compiler::Parser::Parse(source);
        ast.print();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}