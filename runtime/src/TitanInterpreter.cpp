#include "../include/TitanCompiler.h"
#include <iostream>
#include <vector>
#include <stack>
#include <map>

namespace Ardium::Titan::Compiler {

    struct Value {
        bool isString = false;
        int64_t intVal = 0;
        std::string strVal = "";

        Value() : isString(false), intVal(0) {}
        Value(int64_t v) : isString(false), intVal(v) {}
        Value(const std::string& s) : isString(true), intVal(0), strVal(s) {}
    };

    class VirtualMachine {
        const Program& prog;
        size_t ip = 0;
        std::vector<Value> stack;
        std::map<std::string, Value> globals;
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
                        stack.push_back(Value(readI64()));
                        break;
                        
                    case OP_CONST_STR: {
                        stack.push_back(Value(readStr()));
                        break;
                    }

                    case OP_STORE_GLOBAL: {
                        std::string name = readStr();
                        Value val = stack.empty() ? Value(0) : stack.back();
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
                        if (globals.find(name) != globals.end()) {
                            stack.push_back(globals[name]);
                        } else {
                            // Dynamic Variable Fallback
                            stack.push_back(Value(""));
                        }
                        break;
                    }

                    case OP_PRINT_RAW: {
                        if (!stack.empty()) {
                            Value val = stack.back();
                            stack.pop_back();
                            if (val.isString) {
                                std::cout << val.strVal;
                            } else {
                                std::cout << val.intVal;
                            }
                        }
                        break;
                    }

                    case OP_PRINT: {
                        if (!stack.empty()) {
                            Value val = stack.back();
                            stack.pop_back();
                            if (val.isString) {
                                std::cout << val.strVal;
                                if (val.strVal.empty() || val.strVal.back() != '\n') std::cout << std::endl;
                            } else {
                                std::cout << val.intVal << std::endl;
                            }
                        } else {
                            std::cout << std::endl;
                        }
                        break;
                    }
                    
                    // Arithmetic
                    case OP_ADD: {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (a.isString || b.isString) {
                            std::string sa = a.isString ? a.strVal : std::to_string(a.intVal);
                            std::string sb = b.isString ? b.strVal : std::to_string(b.intVal);
                            stack.push_back(Value(sa + sb));
                        } else {
                            stack.push_back(Value(a.intVal + b.intVal));
                        }
                        break;
                    }
                    case OP_SUB: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value(a.intVal - b.intVal)); break; }
                    case OP_MUL: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value(a.intVal * b.intVal)); break; }
                    case OP_DIV: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value(b.intVal==0?0:a.intVal/b.intVal)); break; }
                    case OP_MOD: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value(b.intVal==0?0:a.intVal%b.intVal)); break; }
                    
                    // Comparison
                    case OP_EQ: {
                        Value b=stack.back(); stack.pop_back();
                        Value a=stack.back(); stack.pop_back();
                        int64_t res = (a.isString == b.isString) ? (a.isString ? (a.strVal == b.strVal) : (a.intVal == b.intVal)) : 0;
                        stack.push_back(Value(res));
                        break;
                    }
                    case OP_NE: {
                        Value b=stack.back(); stack.pop_back();
                        Value a=stack.back(); stack.pop_back();
                        int64_t res = (a.isString == b.isString) ? (a.isString ? (a.strVal != b.strVal) : (a.intVal != b.intVal)) : 1;
                        stack.push_back(Value(res));
                        break;
                    }
                    case OP_LT: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value((int64_t)(a.intVal<b.intVal))); break; }
                    case OP_GT: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value((int64_t)(a.intVal>b.intVal))); break; }
                    case OP_LE: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value((int64_t)(a.intVal<=b.intVal))); break; }
                    case OP_GE: { Value b=stack.back(); stack.pop_back(); Value a=stack.back(); stack.pop_back(); stack.push_back(Value((int64_t)(a.intVal>=b.intVal))); break; }
                    
                    // Control Flow
                    case OP_JUMP: {
                        uint32_t dest = readU32();
                        ip = dest;
                        break;
                    }
                    case OP_JUMP_IF_FALSE: {
                        uint32_t dest = readU32();
                        Value cond = stack.back(); stack.pop_back();
                        if (cond.intVal == 0) ip = dest;
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