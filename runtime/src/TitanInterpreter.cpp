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
        std::vector<size_t> callStack;

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
                        
                    case OP_CONST_STR: {
                        uint32_t idx = readU32(); 
                        stack.push_back(idx | 0x4000000000000000); 
                        break;
                    }

                    case OP_STORE_GLOBAL: {
                        std::string name = readStr();
                        int64_t val = stack.empty() ? 0 : stack.back();
                        // Don't pop yet, we might need it? No, store usually pops.
                        if(!stack.empty()) stack.pop_back();
                        globals[name] = val;
                        
                        // Check for handler
                        std::string handlerName = "__handler_" + name;
                        if (prog.functions.count(handlerName)) {
                            uint32_t dest = prog.functions.at(handlerName);
                            callStack.push_back(ip);
                            ip = dest;
                        }
                        break;
                    }
                    
                    case OP_LOAD_GLOBAL: {
                        std::string name = readStr();
                        stack.push_back(globals[name]);
                        break;
                    }

                    case OP_PRINT: {
                        if (!stack.empty()) {
                            int64_t val = stack.back();
                            stack.pop_back();
                            if ((val & 0x4000000000000000) != 0) {
                                uint32_t idx = (uint32_t)(val & 0xFFFFFFFF);
                                if (idx < prog.strings.size()) {
                                    std::cout << prog.strings[idx]; 
                                    if (prog.strings[idx].back() != '\n') std::cout << std::endl;
                                }
                            } else {
                                std::cout << val << std::endl;
                            }
                        } else {
                            std::cout << "" << std::endl;
                        }
                        break;
                    }
                    
                    // Arithmetic
                    case OP_ADD: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a+b); break; }
                    case OP_SUB: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a-b); break; }
                    case OP_MUL: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a*b); break; }
                    case OP_DIV: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(b==0?0:a/b); break; }
                    
                    // Comparison
                    case OP_EQ: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a==b); break; }
                    case OP_NE: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a!=b); break; }
                    case OP_LT: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a<b); break; }
                    case OP_GT: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a>b); break; }
                    case OP_LE: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a<=b); break; }
                    case OP_GE: { int64_t b=stack.back(); stack.pop_back(); int64_t a=stack.back(); stack.pop_back(); stack.push_back(a>=b); break; }
                    
                    // Control Flow
                    case OP_JUMP: {
                        uint32_t dest = readU32();
                        ip = dest;
                        break;
                    }
                    case OP_JUMP_IF_FALSE: {
                        uint32_t dest = readU32();
                        int64_t cond = stack.back(); stack.pop_back();
                        if (cond == 0) ip = dest;
                        break;
                    }
                    
                    case OP_CALL: {
                        std::string funcName = readStr();
                        if (prog.functions.count(funcName)) {
                            uint32_t dest = prog.functions.at(funcName);
                            callStack.push_back(ip);
                            ip = dest;
                        } else {
                            std::cerr << "Runtime Error: Function '" << funcName << "' not found.\n";
                            return;
                        }
                        break;
                    }
                    
                    case OP_RETURN: {
                        if (callStack.empty()) {
                            return; 
                        }
                        ip = callStack.back();
                        callStack.pop_back();
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