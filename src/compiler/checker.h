#pragma once

#include "ast.h"

#include <functional>
#include <map>
#include <set>
#include <unordered_map>

namespace arc {

class Checker {
public:
    Checker(Program& p, Diagnostics& d) : prog_(p), diag_(d), T(Types::get()) {}
    // Returns false if errors were reported.
    bool run(bool testMode);

    static std::string suggest(const std::string& name, const std::vector<std::string>& candidates);

private:
    Program& prog_;
    Diagnostics& diag_;
    Types& T;

    FnDecl* fn_ = nullptr;  // current function (null while checking globals)
    std::vector<std::unordered_map<std::string, Var*>> scopes_;
    std::vector<std::vector<Var*>> scopeVars_;
    std::unordered_map<std::string, Var*> globals_;
    std::vector<Var*> globalOrder_;
    int loopDepth_ = 0;
    Node* curGlobal_ = nullptr;  // global whose initializer is being checked
    std::map<FnDecl*, std::set<Var*>> fnGlobals_;
    std::map<FnDecl*, std::set<FnDecl*>> fnCalls_;
    std::map<Node*, std::set<Var*>> globalDeps_;
    std::map<Node*, std::set<FnDecl*>> globalCalls_;
    struct LambdaCtx { FnDecl* fn; size_t scopeBase; };
    std::vector<LambdaCtx> lambdas_;  // closures being checked, outermost first
    Var* captureInto(FnDecl* L, Var* outer);
    Type* checkLambda(Node* n, Type* expected);
    Type* checkFnArg(Node*& arg, const std::vector<Type*>& params, Type* ret);
    void checkValueArgs(Node* n, size_t firstArg, Type* ft);
    Type* checkTry(Node* n);
    Type* checkPath(Node* n);
    Type* checkMatch(Node* m, Type* expected);
    bool needsReturn(Type* ret);
    bool diverges(Node* b);
    void checkPattern(Node* p, Type* t, bool multi);
    std::vector<std::string> missingPatterns(const std::vector<Node*>& rows, Type* t);
    bool patternError_ = false;
    void noteGlobalUse(Var* v);
    void noteCall(FnDecl* callee);
    void orderGlobals();

    // declarations
    void collectDecls();
    Type* resolveType(TypeExpr* te);
    void resolveSignature(FnDecl* fn);
    void checkFn(FnDecl* fn);
    Type* ensureReturnType(FnDecl* fn, const Loc& useLoc);

    // scopes
    void pushScope();
    void popScope();
    Var* declare(const std::string& name, Type* t, bool mut, const Loc& loc, bool isParam = false);
    Var* lookup(const std::string& name);
    std::vector<std::string> visibleNames();

    // statements
    void stmt(Node* n);
    void block(Node* n, bool newScope = true);
    bool alwaysReturns(Node* n);
    bool containsBreak(Node* n);

    // expressions
    Type* expr(Node* n, Type* expected = nullptr);
    bool coerce(Node*& n, Type* target, const std::string& what = "");
    Type* unify(Node*& a, Node*& b);  // int-literal → float adjustments
    Type* binary(Node* n);
    Type* call(Node* n);
    Type* methodCall(Node* n);
    Type* pathCall(Node* n);
    Type* builtinCall(Node* n);
    Type* builtinMethod(Node* n, Type* recv);
    void checkArgs(Node* n, size_t firstArg, FnDecl* fn, size_t firstParam);
    void requireMutable(Node* target, const Loc& at, const std::string& action);
    bool expectArgs(Node* n, size_t firstArg, size_t count, const std::string& sig);
    bool isComparable(Type* t);
    bool isPrintable(Type* t, const Loc& loc);
    void checkFormatSpec(const std::string& spec, Type* t, const Loc& loc);

    std::string srcText(const Loc& l);
    Diag& err(const Loc& loc, const std::string& msg, const std::string& label = "");
    void mismatch(const Loc& loc, Type* expected, Type* found, const std::string& label = "");
};

}  // namespace arc
