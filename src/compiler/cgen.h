#pragma once

#include "ast.h"

#include <set>
#include <sstream>

namespace arc {

// Lowers a checked Program to a single C translation unit.
class CGen {
public:
    CGen(Program& p, bool testMode) : prog_(p), testMode_(testMode), T(Types::get()) {}
    std::string generate(const std::string& runtimeSrc);

private:
    Program& prog_;
    bool testMode_;
    Types& T;

    // ---- module-level output
    std::map<int, std::string> tdNames_;     // type id -> C descriptor name
    std::vector<Type*> tdOrder_;             // descriptors to emit
    std::map<std::string, int> strLits_;     // literal -> index
    std::vector<std::string> strLitOrder_;

    // ---- per-function state
    struct Temp { std::string name; Type* type; bool cleared = false; };
    std::ostringstream* out_ = nullptr;
    int indent_ = 0;
    int tmpCounter_ = 0;
    std::vector<Temp> temps_;               // declared at the top of the function
    std::vector<Var*> fnVars_;              // non-param locals
    FnDecl* fn_ = nullptr;
    bool hasFrame_ = false;
    int varCounter_ = 0;
    // last `tmp = expr;` line emitted, so `let x = <expr>` can store directly into x
    std::string lastBindName_, lastBindExpr_;
    std::streampos lastBindStart_ = -1, lastBindEnd_ = -1;
    bool storeDirect(const std::string& atom, const std::string& dest);

    // helpers
    static std::string mangle(const std::string& s);
    static std::string cstrLit(const std::string& s);
    std::string ctype(Type* t);
    std::string td(Type* t);
    std::string lit(const std::string& s);
    std::string loc(const Loc& l);
    std::string newTemp(Type* t);
    std::string lval(const std::string& atom, Type* t);
    std::string varName(Var* v);
    std::string fnName(FnDecl* fn);
    std::string structName(StructDecl* sd);
    std::string nilValue(Type* optT);
    std::string someValue(Type* optT, const std::string& v);
    std::string srcText(const Loc& l);
    void line(const std::string& s);
    void open(const std::string& s) { line(s + " {"); indent_++; }
    void close(const std::string& s = "}") { indent_--; line(s); }

    // functions
    std::string genFunction(FnDecl* fn);
    std::string genGlobalInit();
    std::string prototype(FnDecl* fn);
    std::string assembleBody(const std::string& sig, const std::string& body, Type* ret);
    std::string retStmt(const std::string& v);

    // statements
    void stmt(Node* n);
    void stmtInner(Node* n);
    void blockStmts(Node* n, bool clearLocals = true);

    // expressions: emit prerequisite statements, return an atom
    std::string expr(Node* n);
    std::string binaryExpr(Node* n);
    std::string arith(const std::string& op, Type* t, const std::string& a, const std::string& b, const Loc& l);
    std::string eqExpr(Type* t, const std::string& a, const std::string& b);
    std::string callExpr(Node* n);
    std::string builtinCall(Node* n);
    std::string builtinMethod(Node* n);
    void fmtInto(const std::string& buf, const std::string& atom, Type* t, const std::string& spec, bool quoted);
    std::string assign(Node* n);
    std::string bind(Type* t, const std::string& cexpr);  // store C expr in a fresh temp
};

}  // namespace arc
