#include "../include/TitanCompiler.h"
#include <vector>
#include <cctype>
#include <stdexcept>
#include <map>

namespace Ardium::Titan::Compiler {

    // --- CODEGEN HELPERS ---
    
    int addString(Program& prog, const std::string& s) {
        for(size_t i=0; i<prog.strings.size(); ++i) {
            if(prog.strings[i] == s) return (int)i;
        }
        prog.strings.push_back(s);
        return (int)prog.strings.size() - 1;
    }

    void emit(Program& prog, uint8_t op) {
        prog.code.push_back(op);
    }

    void emit64(Program& prog, int64_t val) {
        for(int i=0; i<8; ++i) emit(prog, (val >> (i*8)) & 0xFF);
    }

    void emit32(Program& prog, uint32_t val) {
        for(int i=0; i<4; ++i) emit(prog, (val >> (i*8)) & 0xFF);
    }

    // --- AST IMPLEMENTATIONS ---

    void IntLiteral::codegen(Program& prog) {
        emit(prog, OP_CONST_INT);
        emit64(prog, value);
    }

    void StringLiteral::codegen(Program& prog) {
        emit(prog, OP_CONST_STR);
        emit32(prog, addString(prog, value));
    }

    void VarRef::codegen(Program& prog) {
        emit(prog, OP_LOAD_VAR);
        emit32(prog, addString(prog, name));
    }

    std::string VariableDecl::toString(int indent) const {
        return indentStr(indent) + "let " + name + " = ...";
    }
    void VariableDecl::codegen(Program& prog) {
        if (initializer) initializer->codegen(prog);
        emit(prog, OP_STORE_VAR);
        emit32(prog, addString(prog, name));
    }

    std::string UIContainer::toString(int indent) const { return indentStr(indent) + type + " { ... }"; }
    void UIContainer::codegen(Program& prog) {
        if (type == "VClass" || type == "VStack") emit(prog, OP_UI_VSTACK);
        else if (type == "HClass" || type == "HStack") emit(prog, OP_UI_HSTACK);
        else if (type == "ZClass" || type == "ZStack") emit(prog, OP_UI_ZSTACK);
        
        for (auto& child : children) child->codegen(prog);
        emit(prog, OP_UI_END);
    }

    std::string UIButton::toString(int indent) const { return indentStr(indent) + "Button"; }
    void UIButton::codegen(Program& prog) {
        emit(prog, OP_UI_BUTTON);
        emit32(prog, addString(prog, label));
    }

    std::string PrintStmt::toString(int indent) const { return indentStr(indent) + "println"; }
    void PrintStmt::codegen(Program& prog) {
        if (expr) expr->codegen(prog);
        emit(prog, OP_PRINT);
    }

    std::string FunctionDecl::toString(int indent) const { return indentStr(indent) + "fn " + name; }
    void FunctionDecl::codegen(Program& prog) {
        // Simple main entry point check
        if (name == "main") {
            for (auto& stmt : body) stmt->codegen(prog);
        }
        // Real functions would need jump logic
    }

    // --- PARSER ---

    enum TokenType {
        TOK_EOF, TOK_ID, TOK_INT, TOK_STRING_LIT,
        TOK_LET, TOK_FN, TOK_VCLASS, TOK_HCLASS, TOK_ZCLASS, TOK_BUTTON, TOK_PRINT,
        TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_EQUALS, TOK_SEMICOLON
    };

    struct Token { TokenType type; std::string text; };

    class Lexer {
        std::string src; size_t pos = 0;
    public:
        Lexer(const std::string& s) : src(s) {}
        char peek() { return pos < src.size() ? src[pos] : 0; }
        char advance() { return pos < src.size() ? src[pos++] : 0; }
        
        Token next() {
            while (peek() && isspace(peek())) advance();
            if (pos >= src.size()) return {TOK_EOF, ""};
            
            char c = peek();
            if (isalpha(c)) {
                std::string text;
                while (isalnum(peek()) || peek() == '_') text += advance();
                if (text == "let") return {TOK_LET, text};
                if (text == "fn") return {TOK_FN, text};
                if (text == "VClass" || text == "VStack") return {TOK_VCLASS, text};
                if (text == "HClass" || text == "HStack") return {TOK_HCLASS, text};
                if (text == "ZClass" || text == "ZStack") return {TOK_ZCLASS, text};
                if (text == "Button") return {TOK_BUTTON, text};
                if (text == "println") return {TOK_PRINT, text};
                return {TOK_ID, text};
            }
            if (isdigit(c)) {
                std::string text;
                while (isdigit(peek())) text += advance();
                return {TOK_INT, text};
            }
            if (c == '/') {
                advance();
                if (peek() == '/') {
                    while (peek() != '\n' && peek() != 0) advance();
                    return next();
                }
            }

            if (c == '"') {
                advance(); std::string text;
                while (peek() && peek() != '"') text += advance();
                advance(); return {TOK_STRING_LIT, text};
            }
            if (c == '{') { advance(); return {TOK_LBRACE, "{"}; }
            if (c == '}') { advance(); return {TOK_RBRACE, "}"}; }
            if (c == '(') { advance(); return {TOK_LPAREN, "("}; }
            if (c == ')') { advance(); return {TOK_RPAREN, ")"}; }
            if (c == '=') { advance(); return {TOK_EQUALS, "="}; }
            if (c == ';') { advance(); return {TOK_SEMICOLON, ";"}; }
            
            advance(); return next();
        }
    };

    class ParserImpl {
        Lexer lexer;
        Token current;
    public:
        ParserImpl(const std::string& src) : lexer(src) { current = lexer.next(); }
        void consume(TokenType t) { if (current.type == t) current = lexer.next(); }

        std::shared_ptr<ASTNode> parseStmt() {
            if (current.type == TOK_LET) {
                consume(TOK_LET);
                std::string name = current.text;
                consume(TOK_ID);
                consume(TOK_EQUALS);
                std::shared_ptr<Expression> expr;
                if (current.type == TOK_INT) {
                    expr = std::make_shared<IntLiteral>(std::stoll(current.text));
                    consume(TOK_INT);
                } else if (current.type == TOK_STRING_LIT) {
                    expr = std::make_shared<StringLiteral>(current.text);
                    consume(TOK_STRING_LIT);
                }
                consume(TOK_SEMICOLON);
                return std::make_shared<VariableDecl>(name, false, expr);
            }
            if (current.type == TOK_PRINT) {
                consume(TOK_PRINT);
                consume(TOK_LPAREN);
                std::shared_ptr<Expression> expr;
                if (current.type == TOK_STRING_LIT) {
                    expr = std::make_shared<StringLiteral>(current.text);
                    consume(TOK_STRING_LIT);
                } else if (current.type == TOK_ID) {
                    expr = std::make_shared<VarRef>(current.text);
                    consume(TOK_ID);
                }
                consume(TOK_RPAREN);
                consume(TOK_SEMICOLON);
                return std::make_shared<PrintStmt>(expr);
            }
            if (current.type == TOK_VCLASS || current.type == TOK_HCLASS || current.type == TOK_ZCLASS) {
                std::string type = current.text;
                consume(current.type);
                consume(TOK_LBRACE);
                auto container = std::make_shared<UIContainer>(type);
                while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
                    if (current.type == TOK_BUTTON) {
                        consume(TOK_BUTTON);
                        consume(TOK_LPAREN);
                        std::string label = current.text;
                        consume(TOK_STRING_LIT);
                        consume(TOK_RPAREN);
                        container->children.push_back(std::make_shared<UIButton>(label));
                    }
                }
                consume(TOK_RBRACE);
                return container;
            }
            return nullptr;
        }

        void parse(ProgramAST& ast) {
            while (current.type != TOK_EOF) {
                if (current.type == TOK_FN) {
                    consume(TOK_FN);
                    std::string name = current.text;
                    consume(TOK_ID);
                    consume(TOK_LPAREN); consume(TOK_RPAREN); consume(TOK_LBRACE);
                    auto fn = std::make_shared<FunctionDecl>(name);
                    while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
                        auto stmt = parseStmt();
                        if (stmt) fn->body.push_back(stmt);
                        else current = lexer.next();
                    }
                    consume(TOK_RBRACE);
                    ast.nodes.push_back(fn);
                } else {
                    auto stmt = parseStmt();
                    if (stmt) ast.nodes.push_back(stmt);
                    else current = lexer.next();
                }
            }
        }
    };

    bool Compiler::Compile(const std::string& source, Program& outProgram) {
        try {
            ParserImpl parser(source);
            ProgramAST ast;
            parser.parse(ast);
            
            // Codegen
            for (auto& node : ast.nodes) {
                node->codegen(outProgram);
            }
            emit(outProgram, OP_HALT);
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace Ardium::Titan::Compiler
