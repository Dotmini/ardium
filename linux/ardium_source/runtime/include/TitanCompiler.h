#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <cstdint>

namespace Ardium::Titan::Compiler {

    // --- BYTECODE DEFINITIONS (BACKEND) ---
    enum OpCode : uint8_t {
        OP_HALT = 0,
        OP_CONST_INT,   // [OP] [INT64]
        OP_CONST_STR,   // [OP] [STR_IDX]
        OP_STORE_VAR,   // [OP] [STR_IDX]
        OP_LOAD_VAR,    // [OP] [STR_IDX]
        OP_CALL,        // [OP] [STR_IDX]
        OP_PRINT,       // [OP]
        
        // UI Extensions
        OP_UI_VSTACK,   // [OP]
        OP_UI_HSTACK,   // [OP]
        OP_UI_ZSTACK,   // [OP]
        OP_UI_BUTTON,   // [OP] [STR_IDX]
        OP_UI_END,      // [OP]
    };

    struct Program {
        std::vector<uint8_t> code;
        std::vector<std::string> strings;
        std::unordered_map<std::string, uint32_t> functions;
    };

    // --- AST NODES (FRONTEND) ---

    struct ASTNode {
        virtual ~ASTNode() = default;
        virtual void codegen(Program& prog) = 0; // Added codegen method
        virtual std::string toString(int indent = 0) const = 0;
        std::string indentStr(int i) const { return std::string(i * 2, ' '); }
    };

    struct Expression : ASTNode { };

    struct IntLiteral : Expression {
        int64_t value;
        IntLiteral(int64_t v) : value(v) {}
        std::string toString(int indent) const override { return std::to_string(value); }
        void codegen(Program& prog) override;
    };

    struct StringLiteral : Expression {
        std::string value;
        StringLiteral(const std::string& v) : value(v) {}
        std::string toString(int indent) const override { return "\"" + value + "\""; }
        void codegen(Program& prog) override;
    };

    struct VarRef : Expression {
        std::string name;
        VarRef(const std::string& n) : name(n) {}
        std::string toString(int indent) const override { return name; }
        void codegen(Program& prog) override;
    };

    struct VariableDecl : ASTNode {
        std::string name;
        bool isMutable;
        std::shared_ptr<Expression> initializer;
        VariableDecl(const std::string& n, bool mut, std::shared_ptr<Expression> init) 
            : name(n), isMutable(mut), initializer(init) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct UIContainer : ASTNode {
        std::string type; // VStack, HStack, ZStack
        std::vector<std::shared_ptr<ASTNode>> children;
        UIContainer(const std::string& t) : type(t) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct UIButton : ASTNode {
        std::string label;
        UIButton(const std::string& l) : label(l) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct PrintStmt : ASTNode {
        std::shared_ptr<Expression> expr;
        PrintStmt(std::shared_ptr<Expression> e) : expr(e) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct FunctionDecl : ASTNode {
        std::string name;
        std::vector<std::shared_ptr<ASTNode>> body;
        FunctionDecl(const std::string& n) : name(n) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct ProgramAST {
        std::vector<std::shared_ptr<ASTNode>> nodes;
    };

    class Compiler {
    public:
        static bool Compile(const std::string& source, Program& outProgram);
    };

    class Interpreter {
    public:
        static void Run(const Program& prog);
    };

} // namespace Ardium::Titan::Compiler
