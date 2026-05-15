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
        OP_STORE_GLOBAL,// [OP] [STR_IDX] (Renamed from STORE_VAR)
        OP_LOAD_GLOBAL, // [OP] [STR_IDX] (Renamed from LOAD_VAR)
        OP_STORE_LOCAL, // [OP] [U32_INDEX] (New: Fast Access)
        OP_LOAD_LOCAL,  // [OP] [U32_INDEX] (New: Fast Access)
        OP_CALL,        // [OP] [STR_IDX]
        OP_RETURN,      // [OP]
        OP_POP,         // [OP]
        OP_PRINT,       // [OP]
        
        // Arithmetic & Logic
        OP_ADD, OP_SUB, OP_MUL, OP_DIV,
        OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
        
        // Control Flow
        OP_JUMP,          // [OP] [OFFSET_U32]
        OP_JUMP_IF_FALSE, // [OP] [OFFSET_U32]

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
        virtual void codegen(Program& prog) = 0;
        virtual std::string toString(int indent = 0) const = 0;
        std::string indentStr(int i) const { return std::string(i * 2, ' '); }
    };

    struct Expression : ASTNode { };

    struct ReturnStmt : ASTNode {
        std::shared_ptr<Expression> expr;
        ReturnStmt(std::shared_ptr<Expression> e) : expr(e) {}
        std::string toString(int indent) const override { return "return ..."; }
        void codegen(Program& prog) override;
    };

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
    
    struct BinaryOp : Expression {
        std::string op;
        std::shared_ptr<Expression> left, right;
        BinaryOp(const std::string& o, std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
            : op(o), left(l), right(r) {}
        std::string toString(int indent) const override { return "(" + left->toString(0) + " " + op + " " + right->toString(0) + ")"; }
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
    
    struct Block : ASTNode {
        std::vector<std::shared_ptr<ASTNode>> statements;
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct IfStmt : ASTNode {
        std::shared_ptr<Expression> condition;
        std::shared_ptr<Block> thenBlock;
        std::shared_ptr<ASTNode> elseBlock; // Can be Block or another IfStmt (elif)
        IfStmt(std::shared_ptr<Expression> cond, std::shared_ptr<Block> t, std::shared_ptr<ASTNode> e = nullptr)
            : condition(cond), thenBlock(t), elseBlock(e) {}
        std::string toString(int indent) const override;
        void codegen(Program& prog) override;
    };

    struct WhileStmt : ASTNode {
        std::shared_ptr<Expression> condition;
        std::shared_ptr<Block> body;
        WhileStmt(std::shared_ptr<Expression> cond, std::shared_ptr<Block> b)
            : condition(cond), body(b) {}
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
