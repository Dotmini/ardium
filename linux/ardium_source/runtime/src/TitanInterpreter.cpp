#include "../include/TitanCompiler.h"
#include <iostream>
#include <vector>
#include <stack>
#include <map>

namespace Ardium::Titan::Compiler {

    class VirtualMachine {
        const Program& prog;
        size_t ip = 0;
        std::vector<int64_t> stack;
        std::map<std::string, int64_t> globals;

    public:
        VirtualMachine(const Program& p) : prog(p) {}

        uint8_t readByte() { return prog.code[ip++]; }
        
        uint32_t readU32() {
            uint32_t v = 0;
            for(int i=0; i<4; ++i) v |= ((uint32_t)readByte()) << (i*8);
            return v;
        }

        int64_t readI64() {
            int64_t v = 0;
            for(int i=0; i<8; ++i) v |= ((int64_t)readByte()) << (i*8);
            return v;
        }

        std::string readStr() {
            uint32_t idx = readU32();
            if (idx < prog.strings.size()) return prog.strings[idx];
            return "ERR_STR";
        }

        void run() {
            while (ip < prog.code.size()) {
                OpCode op = (OpCode)readByte();
                switch (op) {
                    case OP_HALT: return;
                    
                    case OP_CONST_INT:
                        stack.push_back(readI64());
                        break;
                        
                    case OP_CONST_STR:
                        // Push dummy val or store somewhere. VM in this demo is int-stack based.
                        // We'll just read the arg to advance IP.
                        readU32(); 
                        break;

                    case OP_STORE_VAR: {
                        std::string name = readStr();
                        int64_t val = stack.empty() ? 0 : stack.back();
                        if(!stack.empty()) stack.pop_back();
                        globals[name] = val;
                        break;
                    }
                    
                    case OP_LOAD_VAR: {
                        std::string name = readStr();
                        stack.push_back(globals[name]);
                        break;
                    }

                    case OP_PRINT: {
                        // In this simple VM, print assumes we are printing "something" contextually
                        // or just a newline if nothing popped.
                        std::cout << "[VM] Print Op" << std::endl;
                        break;
                    }
                    
                    case OP_UI_VSTACK: std::cout << "[VM] UI: Begin VStack\n"; break;
                    case OP_UI_HSTACK: std::cout << "[VM] UI: Begin HStack\n"; break;
                    case OP_UI_ZSTACK: std::cout << "[VM] UI: Begin ZStack\n"; break;
                    
                    case OP_UI_BUTTON: {
                        std::string label = readStr();
                        std::cout << "[VM] UI: Button('" << label << "')\n";
                        break;
                    }
                    
                    case OP_UI_END: std::cout << "[VM] UI: End Container\n"; break;
                    
                    default: break;
                }
            }
        }
    };

    void Interpreter::Run(const Program& prog) {
        VirtualMachine vm(prog);
        vm.run();
    }

} // namespace Ardium::Titan::Compiler