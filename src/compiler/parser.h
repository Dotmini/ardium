#pragma once

#include <algorithm>

#include "lexer.h"

namespace arc {

struct ImportReq {
    std::string path;
    Loc loc;
};

class Parser {
public:
    Parser(SourceFile* f, Diagnostics& d, Program& prog);
    // Parses a whole file, appending items to the program. Returns imports.
    std::vector<ImportReq> parseFile();

private:
    Parser(SourceFile* f, Diagnostics& d, Program& prog, std::vector<Token> toks);

    SourceFile* file_;
    Diagnostics& diag_;
    Program& prog_;
    std::vector<Token> toks_;
    size_t p_ = 0;
    int nest_ = 0;          // >0 inside (), [], {} of expressions: newlines are insignificant
    bool noStruct_ = false; // struct literals disallowed (if/while/for headers)

    const Token& cur() const { return toks_[p_]; }
    const Token& peekTok(size_t k = 1) const { return toks_[std::min(p_ + k, toks_.size() - 1)]; }
    bool isP(const char* s) const { return cur().kind == Tok::Punct && cur().text == s; }
    bool isKw(const char* s) const { return cur().kind == Tok::Keyword && cur().text == s; }
    bool isIdent() const { return cur().kind == Tok::Ident; }
    bool acceptP(const char* s) { if (isP(s)) { p_++; return true; } return false; }
    bool acceptKw(const char* s) { if (isKw(s)) { p_++; return true; } return false; }
    Token next() { Token t = cur(); if (p_ + 1 < toks_.size()) p_++; return t; }

    [[noreturn]] void fail(const Loc& loc, const std::string& msg, const std::string& label = "", const std::string& help = "");
    [[noreturn]] void unexpected(const std::string& what, const std::string& help = "");
    Token expectP(const char* s, const std::string& context);
    Token expectIdent(const std::string& context);
    std::string describe(const Token& t) const;
    void endStmt();
    Loc span(const Loc& a, const Loc& b) const;
    Loc prevEnd() const;

    // items
    void parseItem(std::vector<ImportReq>& imports);
    FnDecl* parseFn(std::vector<std::string> attrs, StructDecl* owner, bool inImpl);
    void parseStruct();
    void parseImpl();
    TypeExpr* parseType();

    // statements
    Node* parseBlock();
    Node* parseStmt();
    Node* parseLet(bool global);
    Node* parseIf();
    Node* parseWhile();
    Node* parseFor();

    // expressions
    Node* parseExpr();
    Node* parseCoalesce();
    Node* parseOr();
    Node* parseAnd();
    Node* parseCmp();
    Node* parseBitOr();
    Node* parseBitXor();
    Node* parseBitAnd();
    Node* parseShift();
    Node* parseAdd();
    Node* parseMul();
    Node* parseCast();
    Node* parseUnary();
    Node* parsePostfix();
    Node* parsePrimary();
    Node* parseBracketLit();
    Node* parseStructLit(const Token& name);
    Node* parseFString(const Token& t);
    std::vector<Node*> parseArgs();
    Node* binary(const std::string& op, Node* l, Node* r);
    bool binOpContinues() const;
};

}  // namespace arc
