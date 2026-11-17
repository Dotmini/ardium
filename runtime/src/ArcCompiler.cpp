#include "../include/ArcCompiler.h"
#include <vector>
#include <cctype>
#include <stdexcept>
#include <map>

namespace Ardium::Arc::Compiler {

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
    
    void patch32(Program& prog, size_t index, uint32_t val) {
        for(int i=0; i<4; ++i) prog.code[index+i] = (val >> (i*8)) & 0xFF;
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
        emit(prog, OP_LOAD_GLOBAL);
        emit32(prog, addString(prog, name));
    }
    
    void BinaryOp::codegen(Program& prog) {
        left->codegen(prog);
        right->codegen(prog);
        if (op == "+") emit(prog, OP_ADD);
        else if (op == "-") emit(prog, OP_SUB);
        else if (op == "*") emit(prog, OP_MUL);
        else if (op == "/") emit(prog, OP_DIV);
        else if (op == "%") emit(prog, OP_MOD);
        else if (op == "==") emit(prog, OP_EQ);
        else if (op == "!=") emit(prog, OP_NE);
        else if (op == "<") emit(prog, OP_LT);
        else if (op == ">") emit(prog, OP_GT);
        else if (op == "<=") emit(prog, OP_LE);
        else if (op == ">=") emit(prog, OP_GE);
    }

    std::string VariableDecl::toString(int indent) const {
        return indentStr(indent) + "let " + name + " = ...";
    }
    void VariableDecl::codegen(Program& prog) {
        if (initializer) initializer->codegen(prog);
        emit(prog, OP_STORE_GLOBAL);
        emit32(prog, addString(prog, name));
    }

    void AssignmentStmt::codegen(Program& prog) {
        if (value) value->codegen(prog);
        emit(prog, OP_STORE_GLOBAL);
        emit32(prog, addString(prog, name));
    }
    
    std::string Block::toString(int indent) const { return indentStr(indent) + "{ ... }"; }
    void Block::codegen(Program& prog) {
        for(auto& stmt : statements) stmt->codegen(prog);
    }

    std::string IfStmt::toString(int indent) const { return indentStr(indent) + "if ..."; }
    void IfStmt::codegen(Program& prog) {
        condition->codegen(prog);
        emit(prog, OP_JUMP_IF_FALSE);
        size_t jumpToElse = prog.code.size();
        emit32(prog, 0); // Placeholder

        thenBlock->codegen(prog);
        
        size_t jumpToEnd = 0;
        if (elseBlock) {
            emit(prog, OP_JUMP);
            jumpToEnd = prog.code.size();
            emit32(prog, 0); // Placeholder
        }
        
        // Patch jumpToElse
        patch32(prog, jumpToElse, (uint32_t)prog.code.size());
        
        if (elseBlock) {
            elseBlock->codegen(prog);
            // Patch jumpToEnd
            patch32(prog, jumpToEnd, (uint32_t)prog.code.size());
        }
    }

    std::string WhileStmt::toString(int indent) const { return indentStr(indent) + "while ..."; }
    void WhileStmt::codegen(Program& prog) {
        size_t loopStart = prog.code.size();
        condition->codegen(prog);
        
        emit(prog, OP_JUMP_IF_FALSE);
        size_t jumpToEnd = prog.code.size();
        emit32(prog, 0); // Placeholder
        
        body->codegen(prog);
        
        emit(prog, OP_JUMP);
        emit32(prog, (uint32_t)loopStart); // Loop back
        
        // Patch jumpToEnd
        patch32(prog, jumpToEnd, (uint32_t)prog.code.size());
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

    std::string PrintStmt::toString(int indent) const { return indentStr(indent) + (isNewline ? "println(...)" : "print(...)"); }
    void PrintStmt::codegen(Program& prog) {
        if (expr) expr->codegen(prog);
        emit(prog, isNewline ? OP_PRINT : OP_PRINT_RAW);
    }

    std::string FunctionDecl::toString(int indent) const { return indentStr(indent) + "fn " + name; }
    void FunctionDecl::codegen(Program& prog) {
        prog.functions[name] = (uint32_t)prog.code.size();
        for (auto& stmt : body) stmt->codegen(prog);
        emit(prog, OP_CONST_INT); emit64(prog, 0);
        emit(prog, OP_RETURN);
    }
    
    void ReturnStmt::codegen(Program& prog) {
        if (expr) expr->codegen(prog);
        else { emit(prog, OP_CONST_INT); emit64(prog, 0); } // Default return 0
        emit(prog, OP_RETURN);
    }

    // --- PARSER ---

    enum TokenType {
        TOK_EOF, TOK_ID, TOK_INT, TOK_STRING_LIT,
        TOK_LET, TOK_FN, TOK_RETURN,
        TOK_VCLASS, TOK_HCLASS, TOK_ZCLASS, TOK_BUTTON, TOK_PRINT,
        TOK_IF, TOK_ELIF, TOK_ELSE, TOK_WHILE,
        TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_EQUALS, TOK_SEMICOLON, TOK_COLON, TOK_COMMA, TOK_DOT, TOK_AT,
        TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_MOD, TOK_ARROW,
        TOK_EQ_EQ, TOK_NOT_EQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE
    };

    struct Token { TokenType type; std::string text; int line = 1; int col = 1; };

    class Lexer {
        std::string src; size_t pos = 0;
        int curLine = 1;
        int curCol = 1;
    public:
        Lexer(const std::string& s) : src(s) {}
        char peek() { return pos < src.size() ? src[pos] : 0; }
        char peekNext() { return pos + 1 < src.size() ? src[pos+1] : 0; }
        char advance() {
            if (pos >= src.size()) return 0;
            char c = src[pos++];
            if (c == '\n') { curLine++; curCol = 1; }
            else { curCol++; }
            return c;
        }
        
        Token next() {
            while (peek() && isspace(peek())) advance();
            if (pos >= src.size()) return {TOK_EOF, "", curLine, curCol};
            
            int tokLine = curLine;
            int tokCol = curCol;
            char c = peek();
            if (isalpha(c) || c == '_') {
                std::string text;
                while (isalnum(peek()) || peek() == '_') text += advance();
                if (text == "let") return {TOK_LET, text, tokLine, tokCol};
                if (text == "fn") return {TOK_FN, text, tokLine, tokCol};
                if (text == "return") return {TOK_RETURN, text, tokLine, tokCol};
                if (text == "if") return {TOK_IF, text, tokLine, tokCol};
                if (text == "elif") return {TOK_ELIF, text, tokLine, tokCol};
                if (text == "else") return {TOK_ELSE, text, tokLine, tokCol};
                if (text == "while") return {TOK_WHILE, text, tokLine, tokCol};
                if (text == "VClass" || text == "VStack") return {TOK_VCLASS, text, tokLine, tokCol};
                if (text == "HClass" || text == "HStack") return {TOK_HCLASS, text, tokLine, tokCol};
                if (text == "ZClass" || text == "ZStack") return {TOK_ZCLASS, text, tokLine, tokCol};
                if (text == "Button") return {TOK_BUTTON, text, tokLine, tokCol};
                if (text == "println") return {TOK_PRINT, text, tokLine, tokCol};
                if (text == "print") return {TOK_PRINT, text, tokLine, tokCol};
                return {TOK_ID, text, tokLine, tokCol};
            }
            if (isdigit(c)) {
                std::string text;
                while (isdigit(peek())) text += advance();
                return {TOK_INT, text, tokLine, tokCol};
            }
            if (c == '-') {
                if (peekNext() == '>') { advance(); advance(); return {TOK_ARROW, "->", tokLine, tokCol}; }
                advance(); return {TOK_MINUS, "-", tokLine, tokCol};
            }
            if (c == '/') {
                if (peekNext() == '/') {
                    while (peek() != '\n' && peek() != 0) advance();
                    return next();
                } else {
                    advance(); return {TOK_SLASH, "/", tokLine, tokCol};
                }
            }
            if (c == '+') { advance(); return {TOK_PLUS, "+", tokLine, tokCol}; }
            if (c == '*') { advance(); return {TOK_STAR, "*", tokLine, tokCol}; }
            if (c == '%') { advance(); return {TOK_MOD, "%", tokLine, tokCol}; }
            
            if (c == '=') { 
                advance(); 
                if (peek() == '=') { advance(); return {TOK_EQ_EQ, "==", tokLine, tokCol}; }
                return {TOK_EQUALS, "=", tokLine, tokCol}; 
            }
            if (c == '!') {
                advance();
                if (peek() == '=') { advance(); return {TOK_NOT_EQ, "!=", tokLine, tokCol}; }
            }
            if (c == '<') {
                advance();
                if (peek() == '=') { advance(); return {TOK_LE, "<=", tokLine, tokCol}; }
                return {TOK_LT, "<", tokLine, tokCol};
            }
            if (c == '>') {
                advance();
                if (peek() == '=') { advance(); return {TOK_GE, ">=", tokLine, tokCol}; }
                return {TOK_GT, ">", tokLine, tokCol};
            }

            if (c == '"') {
                advance(); std::string text;
                while (peek() && peek() != '"') {
                    if (peek() == '\\') {
                        if (peekNext() == 'n') {
                            advance(); advance(); text += '\n';
                        } else if (peekNext() == '"') {
                            advance(); advance(); text += '"';
                        } else {
                            text += advance();
                        }
                    } else {
                        text += advance();
                    }
                }
                advance(); return {TOK_STRING_LIT, text, tokLine, tokCol};
            }
            if (c == '{') { advance(); return {TOK_LBRACE, "{", tokLine, tokCol}; }
            if (c == '}') { advance(); return {TOK_RBRACE, "}", tokLine, tokCol}; }
            if (c == '(') { advance(); return {TOK_LPAREN, "(", tokLine, tokCol}; }
            if (c == ')') { advance(); return {TOK_RPAREN, ")", tokLine, tokCol}; }
            if (c == ';') { advance(); return {TOK_SEMICOLON, ";", tokLine, tokCol}; }
            if (c == ':') { advance(); return {TOK_COLON, ":", tokLine, tokCol}; }
            if (c == ',') { advance(); return {TOK_COMMA, ",", tokLine, tokCol}; }
            if (c == '.') { advance(); return {TOK_DOT, ".", tokLine, tokCol}; }
            if (c == '@') { advance(); return {TOK_AT, "@", tokLine, tokCol}; }
            
            advance(); return next();
        }
    };

    struct CallExpr : Expression {
        std::string callee;
        std::vector<std::shared_ptr<Expression>> args;
        CallExpr(const std::string& c, const std::vector<std::shared_ptr<Expression>>& a) : callee(c), args(a) {}
        std::string toString(int indent) const override { return callee + "(...)"; }
        void codegen(Program& prog) override {
            for (auto& arg : args) arg->codegen(prog);
            emit(prog, OP_CALL);
            emit32(prog, addString(prog, callee));
        }
    };

    class ParserImpl {
        Lexer lexer;
        Token current;
    public:
        ParserImpl(const std::string& src) : lexer(src) { current = lexer.next(); }
        void consume(TokenType t) { 
            if (current.type == t) current = lexer.next(); 
        }

        std::shared_ptr<Expression> parsePrimary() {
            if (current.type == TOK_INT) {
                int64_t v = std::stoll(current.text);
                consume(TOK_INT);
                return std::make_shared<IntLiteral>(v);
            }
            if (current.type == TOK_STRING_LIT) {
                std::string s = current.text;
                consume(TOK_STRING_LIT);
                return std::make_shared<StringLiteral>(s);
            }
            if (current.type == TOK_ID) {
                std::string n = current.text;
                consume(TOK_ID);
                
                // GLOBAL.member access
                if (current.type == TOK_DOT) {
                    consume(TOK_DOT);
                    std::string member = current.text;
                    consume(TOK_ID);
                    if (n == "GLOBAL") {
                        return std::make_shared<VarRef>("GLOBAL_" + member);
                    }
                    return std::make_shared<VarRef>(n + "_" + member);
                }

                // Function Call?
                if (current.type == TOK_LPAREN) {
                    consume(TOK_LPAREN);
                    std::vector<std::shared_ptr<Expression>> args;
                    if (current.type != TOK_RPAREN) {
                        args.push_back(parseExpression());
                        while (current.type == TOK_COMMA) {
                            consume(TOK_COMMA);
                            args.push_back(parseExpression());
                        }
                    }
                    consume(TOK_RPAREN);
                    return std::make_shared<CallExpr>(n, args);
                }
                
                return std::make_shared<VarRef>(n);
            }
            if (current.type == TOK_LPAREN) {
                consume(TOK_LPAREN);
                auto expr = parseExpression();
                if (current.type == TOK_COMMA) {
                    // Tuple expression: concatenate as string representation (e.g. ("v1.0", 8080))
                    std::vector<std::shared_ptr<Expression>> tupleElements;
                    tupleElements.push_back(expr);
                    while (current.type == TOK_COMMA) {
                        consume(TOK_COMMA);
                        tupleElements.push_back(parseExpression());
                    }
                    consume(TOK_RPAREN);
                    std::shared_ptr<Expression> resultExpr = std::make_shared<StringLiteral>("(");
                    for (size_t i = 0; i < tupleElements.size(); ++i) {
                        if (i > 0) {
                            resultExpr = std::make_shared<BinaryOp>("+", resultExpr, std::make_shared<StringLiteral>(", "));
                        }
                        resultExpr = std::make_shared<BinaryOp>("+", resultExpr, tupleElements[i]);
                    }
                    resultExpr = std::make_shared<BinaryOp>("+", resultExpr, std::make_shared<StringLiteral>(")"));
                    return resultExpr;
                }
                consume(TOK_RPAREN);
                return expr;
            }
            return nullptr;
        }

        std::shared_ptr<Expression> parseMultiplicative() {
            auto left = parsePrimary();
            if (!left) return nullptr;

            while (current.type == TOK_STAR || current.type == TOK_SLASH || current.type == TOK_MOD) {
                std::string op = current.text;
                consume(current.type);
                auto right = parsePrimary();
                left = std::make_shared<BinaryOp>(op, left, right);
            }
            return left;
        }

        std::shared_ptr<Expression> parseAdditive() {
            auto left = parseMultiplicative();
            if (!left) return nullptr;

            while (current.type == TOK_PLUS || current.type == TOK_MINUS) {
                std::string op = current.text;
                consume(current.type);
                auto right = parseMultiplicative();
                left = std::make_shared<BinaryOp>(op, left, right);
            }
            return left;
        }

        std::shared_ptr<Expression> parseRelational() {
            auto left = parseAdditive();
            if (!left) return nullptr;

            while (current.type == TOK_LT || current.type == TOK_GT ||
                   current.type == TOK_LE || current.type == TOK_GE) {
                std::string op = current.text;
                consume(current.type);
                auto right = parseAdditive();
                left = std::make_shared<BinaryOp>(op, left, right);
            }
            return left;
        }

        std::shared_ptr<Expression> parseEquality() {
            auto left = parseRelational();
            if (!left) return nullptr;

            while (current.type == TOK_EQ_EQ || current.type == TOK_NOT_EQ) {
                std::string op = current.text;
                consume(current.type);
                auto right = parseRelational();
                left = std::make_shared<BinaryOp>(op, left, right);
            }
            return left;
        }

        std::shared_ptr<Expression> parseExpression() {
            return parseEquality();
        }

        std::shared_ptr<Block> parseBlock() {
            if (current.type == TOK_COLON) consume(TOK_COLON);
            consume(TOK_LBRACE);
            auto block = std::make_shared<Block>();
            while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
                auto stmt = parseStmt();
                if (stmt) block->statements.push_back(stmt);
                else current = lexer.next();
            }
            consume(TOK_RBRACE);
            return block;
        }

        std::shared_ptr<ASTNode> parseIf() {
            consume(TOK_IF);
            auto cond = parseExpression();
            auto thenBlock = parseBlock();
            
            std::shared_ptr<ASTNode> elseNode = nullptr;
            if (current.type == TOK_ELIF) {
                consume(TOK_ELIF);
                auto elifCond = parseExpression();
                auto elifBlock = parseBlock();
                elseNode = std::make_shared<IfStmt>(elifCond, elifBlock, parseElseOrElif());
            } else if (current.type == TOK_ELSE) {
                consume(TOK_ELSE);
                elseNode = parseBlock();
            }
            
            return std::make_shared<IfStmt>(cond, thenBlock, elseNode);
        }

        std::shared_ptr<ASTNode> parseElseOrElif() {
            if (current.type == TOK_ELIF) {
                consume(TOK_ELIF);
                auto cond = parseExpression();
                auto block = parseBlock();
                return std::make_shared<IfStmt>(cond, block, parseElseOrElif());
            }
            if (current.type == TOK_ELSE) {
                consume(TOK_ELSE);
                return parseBlock();
            }
            return nullptr;
        }

        std::shared_ptr<ASTNode> parseWhile() {
            consume(TOK_WHILE);
            auto cond = parseExpression();
            auto body = parseBlock();
            return std::make_shared<WhileStmt>(cond, body);
        }

        std::shared_ptr<ASTNode> parseStmt() {
            if (current.type == TOK_LET) {
                consume(TOK_LET);
                bool isMut = false;
                if (current.text == "mut") { consume(TOK_ID); isMut = true; }
                
                std::string name = current.text;
                consume(TOK_ID);
                
                if (current.type == TOK_COLON) { consume(TOK_COLON); consume(TOK_ID); }

                consume(TOK_EQUALS);
                auto expr = parseExpression();
                consume(TOK_SEMICOLON);
                return std::make_shared<VariableDecl>(name, isMut, expr);
            }
            if (current.type == TOK_RETURN) {
                consume(TOK_RETURN);
                std::shared_ptr<Expression> expr = nullptr;
                if (current.type != TOK_SEMICOLON) {
                    expr = parseExpression();
                }
                consume(TOK_SEMICOLON);
                return std::make_shared<ReturnStmt>(expr);
            }
            if (current.type == TOK_PRINT) {
                bool isPrintln = (current.text == "println");
                consume(TOK_PRINT);
                consume(TOK_LPAREN);
                auto expr = parseExpression();
                consume(TOK_RPAREN);
                if (current.type == TOK_SEMICOLON) consume(TOK_SEMICOLON);
                return std::make_shared<PrintStmt>(expr, isPrintln);
            }
            if (current.type == TOK_IF) return parseIf();
            if (current.type == TOK_WHILE) return parseWhile();
            
            if (current.type == TOK_ID) {
                std::string name = current.text;
                consume(TOK_ID);
                
                // GLOBAL(Config) = ("v1.0", 8080);
                if (name == "GLOBAL" && current.type == TOK_LPAREN) {
                    consume(TOK_LPAREN);
                    std::string key = current.text;
                    consume(TOK_ID);
                    consume(TOK_RPAREN);
                    if (current.type == TOK_EQUALS) {
                        consume(TOK_EQUALS);
                        auto expr = parseExpression();
                        consume(TOK_SEMICOLON);
                        return std::make_shared<AssignmentStmt>("GLOBAL_" + key, expr);
                    }
                }

                if (current.type == TOK_DOT) {
                    // GLOBAL.ID access
                    if (name == "GLOBAL") {
                        consume(TOK_DOT);
                        std::string member = current.text;
                        consume(TOK_ID);
                        if (current.type == TOK_EQUALS) {
                            consume(TOK_EQUALS);
                            auto expr = parseExpression();
                            consume(TOK_SEMICOLON);
                            return std::make_shared<AssignmentStmt>("GLOBAL_" + member, expr);
                        }
                    }
                }
                if (current.type == TOK_EQUALS) {
                    consume(TOK_EQUALS);
                    auto expr = parseExpression();
                    consume(TOK_SEMICOLON);
                    return std::make_shared<AssignmentStmt>(name, expr);
                }
                if (current.type == TOK_LPAREN) {
                    // Direct function call statement: show();
                    consume(TOK_LPAREN);
                    std::vector<std::shared_ptr<Expression>> args;
                    if (current.type != TOK_RPAREN) {
                        args.push_back(parseExpression());
                        while (current.type == TOK_COMMA) {
                            consume(TOK_COMMA);
                            args.push_back(parseExpression());
                        }
                    }
                    consume(TOK_RPAREN);
                    consume(TOK_SEMICOLON);
                    return std::make_shared<CallExpr>(name, args);
                }
            }

            if (current.type == TOK_VCLASS || current.type == TOK_HCLASS || current.type == TOK_ZCLASS) {
                std::string type = current.text;
                consume(current.type);
                if (current.type == TOK_COLON) consume(TOK_COLON);
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
                if (current.type == TOK_AT) {
                    consume(TOK_AT);
                    std::string attr = current.text;
                    consume(TOK_ID);
                    if (attr == "export") {
                        // @export fn ... -> continue to fn
                    } else if (current.type == TOK_LPAREN) {
                        consume(TOK_LPAREN);
                        std::string handlerName = current.text;
                        consume(TOK_ID);
                        consume(TOK_RPAREN);
                        
                        consume(TOK_LBRACE);
                        auto fn = std::make_shared<FunctionDecl>("__handler_" + handlerName);
                        while (current.type != TOK_RBRACE && current.type != TOK_EOF) {
                            auto stmt = parseStmt();
                            if (stmt) fn->body.push_back(stmt);
                            else current = lexer.next();
                        }
                        consume(TOK_RBRACE);
                        ast.nodes.push_back(fn);
                        continue;
                    }
                }

                if (current.type == TOK_FN) {
                    consume(TOK_FN);
                    std::string name = current.text;
                    consume(TOK_ID);
                    if (current.type == TOK_LPAREN) {
                        consume(TOK_LPAREN);
                        consume(TOK_RPAREN);
                    }
                    
                    // Optional return type syntax "-> Type"
                    if (current.type == TOK_ARROW) {
                        consume(TOK_ARROW);
                        consume(TOK_ID); // Consume Type Name
                    }

                    if (current.type == TOK_COLON) consume(TOK_COLON);
                    
                    consume(TOK_LBRACE);
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
            
            // 1. Initialize top-level variables & statements first
            for (auto& node : ast.nodes) {
                if (dynamic_cast<VariableDecl*>(node.get())) {
                    node->codegen(outProgram);
                }
            }

            // 2. Bootstrap: Jump to main
            emit(outProgram, OP_CALL); 
            emit32(outProgram, addString(outProgram, "main"));
            emit(outProgram, OP_HALT); // Stop after main returns

            // 3. Codegen function bodies
            for (auto& node : ast.nodes) {
                if (!dynamic_cast<VariableDecl*>(node.get())) {
                    node->codegen(outProgram);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::string sanitizeIdent(const std::string& n) {
        return "ar_" + n;
    }

    static std::string exprToCpp(const std::shared_ptr<Expression>& expr) {
        if (!expr) return "0";
        if (auto lit = std::dynamic_pointer_cast<IntLiteral>(expr)) {
            return std::to_string(lit->value);
        }
        if (auto slit = std::dynamic_pointer_cast<StringLiteral>(expr)) {
            return "std::string(\"" + slit->value + "\")";
        }
        if (auto vref = std::dynamic_pointer_cast<VarRef>(expr)) {
            if (vref->name.rfind("GLOBAL_", 0) == 0) {
                return "_global_vars[\"" + vref->name.substr(7) + "\"]";
            }
            return sanitizeIdent(vref->name);
        }
        if (auto bin = std::dynamic_pointer_cast<BinaryOp>(expr)) {
            return "(" + exprToCpp(bin->left) + " " + bin->op + " " + exprToCpp(bin->right) + ")";
        }
        if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
            std::string s = call->callee + "(";
            for (size_t i = 0; i < call->args.size(); ++i) {
                if (i > 0) s += ", ";
                s += exprToCpp(call->args[i]);
            }
            s += ")";
            return s;
        }
        return "0";
    }

    static void stmtToCpp(const std::shared_ptr<ASTNode>& node, std::stringstream& ss, int indent) {
        std::string ind(indent * 4, ' ');
        if (!node) return;
        if (auto vdecl = std::dynamic_pointer_cast<VariableDecl>(node)) {
            if (indent == 0) {
                ss << "auto " << sanitizeIdent(vdecl->name) << " = " << (vdecl->initializer ? exprToCpp(vdecl->initializer) : "0") << ";\n";
            } else {
                ss << ind << "auto " << sanitizeIdent(vdecl->name) << " = " << (vdecl->initializer ? exprToCpp(vdecl->initializer) : "0") << ";\n";
            }
            return;
        }
        if (auto asgn = std::dynamic_pointer_cast<AssignmentStmt>(node)) {
            if (asgn->name.rfind("GLOBAL_", 0) == 0) {
                ss << ind << "_global_vars[\"" << asgn->name.substr(7) << "\"] = " << exprToCpp(asgn->value) << ";\n";
            } else {
                ss << ind << sanitizeIdent(asgn->name) << " = " << exprToCpp(asgn->value) << ";\n";
            }
            return;
        }
        if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(node)) {
            ss << ind << "return " << (ret->expr ? exprToCpp(ret->expr) : "") << ";\n";
            return;
        }
        if (auto prn = std::dynamic_pointer_cast<PrintStmt>(node)) {
            ss << ind << "std::cout << " << (prn->expr ? exprToCpp(prn->expr) : "\"\"") << (prn->isNewline ? " << std::endl;\n" : ";\n");
            return;
        }
        if (auto whl = std::dynamic_pointer_cast<WhileStmt>(node)) {
            ss << ind << "while (" << exprToCpp(whl->condition) << ") {\n";
            if (whl->body) {
                for (auto& s : whl->body->statements) {
                    stmtToCpp(s, ss, indent + 1);
                }
            }
            ss << ind << "}\n";
            return;
        }
        if (auto ifs = std::dynamic_pointer_cast<IfStmt>(node)) {
            ss << ind << "if (" << exprToCpp(ifs->condition) << ") {\n";
            if (ifs->thenBlock) {
                for (auto& s : ifs->thenBlock->statements) {
                    stmtToCpp(s, ss, indent + 1);
                }
            }
            if (ifs->elseBlock) {
                ss << ind << "} else {\n";
                if (auto elif = std::dynamic_pointer_cast<IfStmt>(ifs->elseBlock)) {
                    stmtToCpp(elif, ss, indent + 1);
                } else if (auto blk = std::dynamic_pointer_cast<Block>(ifs->elseBlock)) {
                    for (auto& s : blk->statements) {
                        stmtToCpp(s, ss, indent + 1);
                    }
                }
                ss << ind << "}\n";
            } else {
                ss << ind << "}\n";
            }
            return;
        }
        if (auto call = std::dynamic_pointer_cast<CallExpr>(node)) {
            if (call->callee == "println") {
                ss << ind << "std::cout << " << (call->args.empty() ? "\"\"" : exprToCpp(call->args[0])) << " << std::endl;\n";
            } else if (call->callee == "print") {
                ss << ind << "std::cout << " << (call->args.empty() ? "\"\"" : exprToCpp(call->args[0])) << ";\n";
            } else {
                ss << ind << exprToCpp(call) << ";\n";
            }
            return;
        }
    }

    bool Compiler::CompileToNativeCpp(const std::string& source, std::string& outCpp) {
        try {
            ParserImpl parser(source);
            ProgramAST ast;
            parser.parse(ast);

            std::stringstream ss;
            ss << "#include <iostream>\n#include <string>\n#include <cstdint>\n#include <type_traits>\n#include <unordered_map>\n\n";
            ss << "inline std::unordered_map<std::string, std::string> _global_vars;\n";
            ss << "inline std::string operator+(const std::string& s, int64_t v) { return s + std::to_string(v); }\n";
            ss << "inline std::string operator+(int64_t v, const std::string& s) { return std::to_string(v) + s; }\n";
            ss << "inline std::string operator+(const std::string& s, int v) { return s + std::to_string(v); }\n";
            ss << "inline std::string operator+(int v, const std::string& s) { return std::to_string(v) + s; }\n\n";

            // 1. Top-level variables & statements
            for (auto& node : ast.nodes) {
                if (!dynamic_cast<FunctionDecl*>(node.get())) {
                    stmtToCpp(node, ss, 0);
                }
            }
            ss << "\n";

            // 2. Forward function declarations
            for (auto& node : ast.nodes) {
                if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(node)) {
                    if (fn->name != "main") {
                        ss << "auto " << fn->name << "();\n";
                    }
                }
            }
            ss << "\n";

            // 3. Function implementations
            for (auto& node : ast.nodes) {
                if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(node)) {
                    if (fn->name == "main") {
                        ss << "int main() {\n";
                    } else {
                        ss << "auto " << fn->name << "() {\n";
                    }
                    for (auto& s : fn->body) {
                        stmtToCpp(s, ss, 1);
                    }
                    if (fn->name == "main") {
                        ss << "    return 0;\n";
                    }
                    ss << "}\n\n";
                }
            }
            outCpp = ss.str();
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace Ardium::Arc::Compiler