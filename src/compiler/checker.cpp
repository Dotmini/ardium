#include "checker.h"

#include <algorithm>
#include <set>

namespace arc {

// ---------------------------------------------------------------- helpers --

// Optimal-string-alignment distance (adjacent transpositions cost 1), case-insensitive.
static size_t editDistance(const std::string& a, const std::string& b) {
    size_t n = a.size(), m = b.size();
    std::vector<std::vector<size_t>> d(n + 1, std::vector<size_t>(m + 1));
    for (size_t i = 0; i <= n; i++) d[i][0] = i;
    for (size_t j = 0; j <= m; j++) d[0][j] = j;
    auto lc = [](char c) { return (char)std::tolower((unsigned char)c); };
    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            size_t cost = lc(a[i - 1]) == lc(b[j - 1]) ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
            if (i > 1 && j > 1 && lc(a[i - 1]) == lc(b[j - 2]) && lc(a[i - 2]) == lc(b[j - 1]))
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
        }
    }
    return d[n][m];
}

std::string Checker::suggest(const std::string& name, const std::vector<std::string>& candidates) {
    std::string best;
    size_t limit = std::max<size_t>(1, (name.size() + 2) / 3);
    size_t bestD = limit + 1;
    for (const auto& c : candidates) {
        if (c == name) continue;
        size_t d = editDistance(name, c);
        if (d < bestD) { bestD = d; best = c; }
    }
    return best;
}

Diag& Checker::err(const Loc& loc, const std::string& msg, const std::string& label) {
    return diag_.error(loc, msg, label);
}

void Checker::mismatch(const Loc& loc, Type* expected, Type* found, const std::string& label) {
    if (expected->isError() || found->isError()) return;
    Diag& d = err(loc, "mismatched types: expected `" + expected->str() + "`, found `" + found->str() + "`",
                  label.empty() ? "expected `" + expected->str() + "`" : label);
    if (expected->kind == TK::Float && found->kind == TK::Int)
        d.notes.push_back("help: convert the value with `float(x)` or `x as float`");
    else if (expected->kind == TK::Int && found->kind == TK::Float)
        d.notes.push_back("help: convert the value with `int(x)` (truncates) or `round(x) as int`");
    else if (expected->kind == TK::Str && (found->kind == TK::Int || found->kind == TK::Float || found->kind == TK::Bool))
        d.notes.push_back("help: convert the value with `str(x)`");
    else if (found->kind == TK::Nil)
        d.notes.push_back("help: only optional types can hold `nil`; use `?" + expected->str() + "`");
    else if (found->kind == TK::Optional && found->elem == expected)
        d.notes.push_back("help: unwrap the optional first: `if let v = x { ... }`, `x ?? default` or `x.unwrap()`");
    else if (expected->kind == TK::Bool && found->kind == TK::Int)
        d.notes.push_back("help: Arc has no implicit truthiness; compare explicitly, e.g. `x != 0`");
}

static const std::vector<std::string> kBuiltinFns = {
    "print", "println", "eprint", "eprintln", "str", "int", "float", "len", "parse_int", "parse_float",
    "abs", "min", "max", "sqrt", "pow", "floor", "ceil", "round", "sin", "cos", "tan", "atan2", "exp", "log",
    "log10", "ipow", "assert", "assert_eq", "panic", "exit", "read_line", "read_file", "write_file",
    "append_file", "file_exists", "args", "time_ns", "time_ms", "unix_time", "sleep_ms", "rand_int",
    "rand_float", "seed", "chr", "ord", "getenv", "run", "gc_count",
};

// ------------------------------------------------------------------- run --

bool Checker::run(bool testMode) {
    collectDecls();
    if (diag_.hasErrors()) return false;

    for (FnDecl* fn : prog_.fns) resolveSignature(fn);
    for (FnDecl* fn : prog_.methods) resolveSignature(fn);
    if (diag_.hasErrors()) return false;

    // globals (checked on demand by lookup; this forces the rest in order)
    for (Node* g : prog_.globals) {
        if (!g->var) {
            std::string name = g->str;
            lookup(name);
        }
    }
    for (FnDecl* fn : prog_.fns) checkFn(fn);
    for (FnDecl* fn : prog_.methods) checkFn(fn);

    if (!testMode) {
        auto it = prog_.fnByName.find("main");
        if (it == prog_.fnByName.end()) {
            Loc l;
            if (!prog_.files.empty()) { l.file = 0; l.line = 1; l.col = 1; }
            Diag& d = err(l, "`main` function not found");
            d.notes.push_back("help: add an entry point: `fn main() { println(\"hello\") }`");
        } else {
            FnDecl* m = it->second;
            prog_.mainFn = m;
            if (!m->params.empty())
                err(m->loc, "`main` cannot take parameters").notes.push_back("help: use `args()` to read command-line arguments");
            if (m->ret && m->ret->kind != TK::Void && m->ret->kind != TK::Int && !m->ret->isError())
                err(m->loc, "`main` must return `int` or nothing, not `" + m->ret->str() + "`");
        }
    }
    for (FnDecl* fn : prog_.fns) {
        if (!fn->isTest) continue;
        if (!fn->params.empty()) err(fn->loc, "test functions cannot take parameters");
        if (fn->ret && fn->ret->kind != TK::Void && !fn->ret->isError()) err(fn->loc, "test functions must not return a value");
    }
    return !diag_.hasErrors();
}

void Checker::collectDecls() {
    static const std::set<std::string> reserved = {"int", "float", "bool", "str", "string", "void", "i64", "f64"};
    for (StructDecl* sd : prog_.structs) {
        if (reserved.count(sd->name)) { err(sd->loc, "`" + sd->name + "` is a built-in type name"); continue; }
        if (prog_.structByName.count(sd->name)) {
            err(sd->loc, "the struct `" + sd->name + "` is defined multiple times");
            continue;
        }
        prog_.structByName[sd->name] = sd;
        sd->type = T.structT(sd);
    }
    for (StructDecl* sd : prog_.structs)
        for (FieldDecl& f : sd->fields) f.type = resolveType(f.te);

    for (FnDecl* fn : prog_.methods) {
        StructDecl* placeholder = fn->owner;
        auto it = prog_.structByName.find(placeholder->name);
        if (it == prog_.structByName.end()) {
            std::vector<std::string> names;
            for (auto& kv : prog_.structByName) names.push_back(kv.first);
            Diag& d = err(placeholder->loc, "cannot find struct `" + placeholder->name + "` for this `impl`", "not found");
            std::string s = suggest(placeholder->name, names);
            if (!s.empty()) d.notes.push_back("help: a struct with a similar name exists: `" + s + "`");
            continue;
        }
        StructDecl* sd = it->second;
        fn->owner = sd;
        if (sd->methods.count(fn->name)) {
            err(fn->loc, "duplicate definition of method `" + fn->name + "` for `" + sd->name + "`");
            continue;
        }
        sd->methods[fn->name] = fn;
    }

    for (FnDecl* fn : prog_.fns) {
        if (prog_.fnByName.count(fn->name)) {
            Diag& d = err(fn->loc, "the function `" + fn->name + "` is defined multiple times", "redefined here");
            FnDecl* prev = prog_.fnByName[fn->name];
            d.notes.push_back("note: previous definition at line " + std::to_string(prev->loc.line));
            continue;
        }
        prog_.fnByName[fn->name] = fn;
    }
}

Type* Checker::resolveType(TypeExpr* te) {
    switch (te->kind) {
    case TypeExpr::Name: {
        const std::string& n = te->name;
        if (n == "int" || n == "i64" || n == "Int") return T.intT;
        if (n == "float" || n == "f64" || n == "Float" || n == "double") return T.floatT;
        if (n == "bool" || n == "Bool") return T.boolT;
        if (n == "str" || n == "string" || n == "String" || n == "Str") return T.strT;
        if (n == "void") return T.voidT;
        auto it = prog_.structByName.find(n);
        if (it != prog_.structByName.end()) return it->second->type;
        std::vector<std::string> names = {"int", "float", "bool", "str"};
        for (auto& kv : prog_.structByName) names.push_back(kv.first);
        Diag& d = err(te->loc, "cannot find type `" + n + "`", "not found");
        std::string s = suggest(n, names);
        if (!s.empty()) d.notes.push_back("help: did you mean `" + s + "`?");
        return T.errorT;
    }
    case TypeExpr::Array: {
        Type* e = resolveType(te->a);
        if (e->kind == TK::Void) { err(te->a->loc, "arrays cannot hold `void`"); return T.errorT; }
        return e->isError() ? e : T.array(e);
    }
    case TypeExpr::Map: {
        Type* k = resolveType(te->a);
        Type* v = resolveType(te->b);
        if (k->isError() || v->isError()) return T.errorT;
        if (k->kind != TK::Int && k->kind != TK::Str && k->kind != TK::Bool) {
            err(te->a->loc, "map keys must be `int`, `str` or `bool`, not `" + k->str() + "`");
            return T.errorT;
        }
        if (v->kind == TK::Void) { err(te->b->loc, "maps cannot hold `void`"); return T.errorT; }
        return T.map(k, v);
    }
    case TypeExpr::Optional: {
        Type* e = resolveType(te->a);
        if (e->isError()) return e;
        if (e->kind == TK::Optional) { err(te->loc, "nested optionals (`??T`) are not supported"); return T.errorT; }
        if (e->kind == TK::Void) { err(te->loc, "`?void` is not a valid type"); return T.errorT; }
        return T.optional(e);
    }
    }
    return T.errorT;
}

void Checker::resolveSignature(FnDecl* fn) {
    for (Param& p : fn->params) fn->paramTypes.push_back(resolveType(p.te));
    if (fn->retTe) fn->ret = resolveType(fn->retTe);
    else fn->retFromBody = true;
    std::set<std::string> seen;
    for (Param& p : fn->params) {
        if (!seen.insert(p.name).second) err(p.loc, "parameter `" + p.name + "` is declared more than once");
    }
}

Type* Checker::ensureReturnType(FnDecl* fn, const Loc& useLoc) {
    if (fn->ret) return fn->ret;
    if (fn->state == 0) {
        checkFn(fn);
        if (fn->ret) return fn->ret;
    }
    if (fn->state == 1) {
        Diag& d = err(useLoc, "cannot infer the return type of `" + fn->displayName() + "` before it is known",
                      "recursive call");
        d.notes.push_back("help: declare the return type explicitly: `fn " + fn->name + "(...) -> int`");
        fn->ret = T.errorT;
        return T.errorT;
    }
    return fn->ret ? fn->ret : T.errorT;
}

static bool hasValueReturn(Node* n) {
    if (!n) return false;
    if (n->kind == NK::Return) return !n->kids.empty();
    for (Node* k : n->kids) if (hasValueReturn(k)) return true;
    return false;
}

void Checker::checkFn(FnDecl* fn) {
    if (fn->state != 0) return;
    fn->state = 1;
    // a function without `return <value>` statements returns nothing; knowing this
    // up front lets recursive calls to it type-check
    if (!fn->ret && !hasValueReturn(fn->body)) fn->ret = T.voidT;
    // save context (may be invoked on demand from another function)
    FnDecl* savedFn = fn_;
    auto savedScopes = std::move(scopes_);
    auto savedScopeVars = std::move(scopeVars_);
    int savedLoop = loopDepth_;
    scopes_.clear();
    scopeVars_.clear();
    loopDepth_ = 0;
    fn_ = fn;

    pushScope();
    if (fn->hasSelf && fn->owner) {
        fn->selfVar = declare("self", fn->owner->type, fn->selfMut, fn->loc, true);
        fn->selfVar->used = true;
    }
    for (size_t i = 0; i < fn->params.size(); i++) {
        Param& p = fn->params[i];
        p.var = declare(p.name, fn->paramTypes[i], p.mut, p.loc, true);
    }
    block(fn->body, false);
    popScope();

    if (!fn->ret) fn->ret = T.voidT;
    if (fn->ret->kind != TK::Void && !fn->ret->isError() && !alwaysReturns(fn->body)) {
        Diag& d = err(fn->loc, "function `" + fn->displayName() + "` might not return a value on every path",
                      "expected to return `" + fn->ret->str() + "`");
        d.notes.push_back("help: add a `return` statement at the end of the function");
    }

    fn_ = savedFn;
    scopes_ = std::move(savedScopes);
    scopeVars_ = std::move(savedScopeVars);
    loopDepth_ = savedLoop;
    fn->state = 2;
}

// ----------------------------------------------------------------- scopes --

void Checker::pushScope() {
    scopes_.emplace_back();
    scopeVars_.emplace_back();
}

void Checker::popScope() {
    for (Var* v : scopeVars_.back()) {
        if (v->name.empty() || v->name[0] == '_' || v->name == "self") continue;
        if (!v->used) {
            Diag& d = diag_.warning(v->loc, "unused " + std::string(v->isParam ? "parameter" : "variable") + " `" + v->name + "`");
            d.notes.push_back("help: if this is intentional, prefix it with an underscore: `_" + v->name + "`");
        } else if (v->mut && !v->assigned && !v->isParam) {
            Diag& d = diag_.warning(v->loc, "variable `" + v->name + "` is declared `mut` but never modified");
            d.notes.push_back("help: remove `mut`");
        }
    }
    scopes_.pop_back();
    scopeVars_.pop_back();
}

Var* Checker::declare(const std::string& name, Type* t, bool mut, const Loc& loc, bool isParam) {
    Var* v = new Var();
    v->name = name;
    v->type = t;
    v->mut = mut;
    v->loc = loc;
    v->isParam = isParam;
    scopes_.back()[name] = v;
    scopeVars_.back().push_back(v);
    if (fn_) fn_->locals.push_back(v);
    return v;
}

Var* Checker::lookup(const std::string& name) {
    for (size_t i = scopes_.size(); i-- > 0;) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return it->second;
    }
    auto g = globals_.find(name);
    if (g != globals_.end()) return g->second;
    // on-demand global initialization check
    for (Node* let : prog_.globals) {
        if (let->str != name) continue;
        if (let->isConst) {  // in progress
            err(let->loc, "the initializer of global `" + name + "` refers to itself");
            return nullptr;
        }
        if (let->var) return let->var;
        let->isConst = true;
        FnDecl* savedFn = fn_;
        auto savedScopes = std::move(scopes_);
        auto savedScopeVars = std::move(scopeVars_);
        int savedLoop = loopDepth_;
        scopes_.clear();
        scopeVars_.clear();
        fn_ = nullptr;
        loopDepth_ = 0;
        Type* declared = let->tyExpr ? resolveType(let->tyExpr) : nullptr;
        Type* t = expr(let->kids[0], declared);
        if (declared) { coerce(let->kids[0], declared, "global initializer"); t = declared; }
        if (t->kind == TK::Nil) {
            err(let->loc, "cannot infer the type of `" + name + "`").notes.push_back("help: add a type: `let " + name + ": ?int = nil`");
            t = T.errorT;
        } else if (t->kind == TK::Void) {
            err(let->kids[0]->loc, "this expression does not produce a value");
            t = T.errorT;
        }
        fn_ = savedFn;
        scopes_ = std::move(savedScopes);
        scopeVars_ = std::move(savedScopeVars);
        loopDepth_ = savedLoop;
        Var* v = new Var();
        v->name = name;
        v->type = t;
        v->mut = let->flag;
        v->loc = let->loc;
        v->isGlobal = true;
        v->used = true;
        let->var = v;
        let->type = t;
        let->isConst = false;
        globals_[name] = v;
        globalOrder_.push_back(v);
        prog_.globalInitOrder.push_back(let);
        return v;
    }
    return nullptr;
}

std::vector<std::string> Checker::visibleNames() {
    std::vector<std::string> names;
    for (auto& s : scopes_) for (auto& kv : s) names.push_back(kv.first);
    for (Node* g : prog_.globals) names.push_back(g->str);
    return names;
}

// ------------------------------------------------------------- statements --

void Checker::block(Node* n, bool newScope) {
    if (newScope) pushScope();
    for (Node* s : n->kids) stmt(s);
    if (newScope) popScope();
}

bool Checker::containsBreak(Node* n) {
    if (!n) return false;
    switch (n->kind) {
    case NK::Break: return true;
    case NK::While: case NK::Loop: case NK::ForRange: case NK::ForEach: return false;
    default:
        for (Node* k : n->kids) if (containsBreak(k)) return true;
        return false;
    }
}

bool Checker::alwaysReturns(Node* n) {
    if (!n) return false;
    switch (n->kind) {
    case NK::Return: return true;
    case NK::Block:
        for (Node* k : n->kids) if (alwaysReturns(k)) return true;
        return false;
    case NK::If: case NK::IfLet:
        return n->kids[2] && alwaysReturns(n->kids[1]) && alwaysReturns(n->kids[2]);
    case NK::Loop: return !containsBreak(n->kids[0]);
    case NK::While:
        return n->kids[0]->kind == NK::BoolLit && n->kids[0]->flag && !containsBreak(n->kids[1]);
    case NK::ExprStmt: {
        Node* e = n->kids[0];
        return e->kind == NK::Call && (e->builtin == BI::Panic || e->builtin == BI::Exit);
    }
    default: return false;
    }
}

void Checker::stmt(Node* n) {
    switch (n->kind) {
    case NK::Let: {
        Type* declared = n->tyExpr ? resolveType(n->tyExpr) : nullptr;
        Type* t = expr(n->kids[0], declared);
        if (declared) {
            coerce(n->kids[0], declared, "");
            t = declared;
        } else if (t->kind == TK::Nil) {
            Diag& d = err(n->kids[0]->loc, "cannot infer the type of `" + n->str + "` from `nil`");
            d.notes.push_back("help: add a type annotation: `let " + std::string(n->flag ? "mut " : "") + n->str + ": ?int = nil`");
            t = T.errorT;
        } else if (t->kind == TK::Void) {
            err(n->kids[0]->loc, "this expression does not produce a value", "returns nothing");
            t = T.errorT;
        }
        n->var = declare(n->str, t, n->flag, n->loc);
        n->type = t;
        return;
    }
    case NK::Assign: {
        Node* target = n->kids[0];
        if (target->kind != NK::Ident && target->kind != NK::Field && target->kind != NK::Index) {
            err(target->loc, "invalid left-hand side of assignment", "cannot assign to this expression");
            expr(n->kids[1]);
            return;
        }
        Type* tt = expr(target);
        if (target->kind == NK::Index && target->kids[0]->type && target->kids[0]->type->kind == TK::Str) {
            err(target->loc, "strings are immutable; cannot assign to a character");
            return;
        }
        requireMutable(target, target->loc, "assign to");
        std::string op = n->str;
        if (op == "=") {
            expr(n->kids[1], tt);
            coerce(n->kids[1], tt, "");
            return;
        }
        // compound assignment: type-check as a binary operation
        std::string bop = op.substr(0, op.size() - 1);
        Type* vt = expr(n->kids[1], tt);
        if (tt->isError() || vt->isError()) return;
        if (tt->kind == TK::Float && n->kids[1]->kind == NK::IntLit) coerce(n->kids[1], tt);
        vt = n->kids[1]->type;
        bool ok = false;
        if (bop == "+" && tt->kind == TK::Str && vt->kind == TK::Str) ok = true;
        else if ((bop == "+" || bop == "-" || bop == "*" || bop == "/" || bop == "%") && tt->isNumeric() && tt == vt) ok = true;
        else if ((bop == "&" || bop == "|" || bop == "^" || bop == "<<" || bop == ">>") && tt->kind == TK::Int && vt->kind == TK::Int) ok = true;
        if (!ok) {
            Diag& d = err(n->loc, "cannot apply `" + op + "` to `" + tt->str() + "` and `" + vt->str() + "`");
            if (tt->kind == TK::Str) d.notes.push_back("help: convert the value with `str(x)`");
        }
        return;
    }
    case NK::ExprStmt: {
        Node* e = n->kids[0];
        expr(e);
        bool effect = e->kind == NK::Call || e->kind == NK::MethodCall || e->kind == NK::PathCall;
        if (!effect && !(e->type && e->type->isError())) {
            Diag& d = diag_.warning(e->loc, "expression result is unused", "this value is discarded");
            if (e->kind == NK::Binary && e->str == "==") d.notes.push_back("help: did you mean to assign with `=`?");
        }
        return;
    }
    case NK::If: {
        Type* c = expr(n->kids[0], T.boolT);
        if (c->kind != TK::Bool && !c->isError()) mismatch(n->kids[0]->loc, T.boolT, c, "expected `bool` condition");
        block(n->kids[1]);
        if (n->kids[2]) {
            if (n->kids[2]->kind == NK::Block) block(n->kids[2]);
            else stmt(n->kids[2]);
        }
        return;
    }
    case NK::IfLet: {
        Type* t = expr(n->kids[0]);
        Type* inner = T.errorT;
        if (t->kind == TK::Optional) inner = t->elem;
        else if (!t->isError()) {
            Diag& d = err(n->kids[0]->loc, "`if let` requires an optional value, found `" + t->str() + "`");
            d.notes.push_back("help: optional types are written `?T`, e.g. `?int`");
        }
        pushScope();
        n->var = declare(n->str, inner, false, n->nameLocs.empty() ? n->loc : n->nameLocs[0]);
        block(n->kids[1], false);
        popScope();
        if (n->kids[2]) {
            if (n->kids[2]->kind == NK::Block) block(n->kids[2]);
            else stmt(n->kids[2]);
        }
        return;
    }
    case NK::While: {
        Type* c = expr(n->kids[0], T.boolT);
        if (c->kind != TK::Bool && !c->isError()) mismatch(n->kids[0]->loc, T.boolT, c, "expected `bool` condition");
        loopDepth_++;
        block(n->kids[1]);
        loopDepth_--;
        return;
    }
    case NK::Loop:
        loopDepth_++;
        block(n->kids[0]);
        loopDepth_--;
        return;
    case NK::ForRange: {
        Type* a = expr(n->kids[0], T.intT);
        Type* b = expr(n->kids[1], T.intT);
        if (a->kind != TK::Int && !a->isError()) mismatch(n->kids[0]->loc, T.intT, a, "range bounds must be `int`");
        if (b->kind != TK::Int && !b->isError()) mismatch(n->kids[1]->loc, T.intT, b, "range bounds must be `int`");
        pushScope();
        n->var = declare(n->str, T.intT, false, n->nameLocs[0]);
        loopDepth_++;
        block(n->kids[2], false);
        loopDepth_--;
        popScope();
        return;
    }
    case NK::ForEach: {
        Type* it = expr(n->kids[0]);
        Type* t1 = T.errorT;
        Type* t2 = T.errorT;
        bool two = n->names.size() == 2;
        if (it->kind == TK::Array) { t1 = two ? T.intT : it->elem; t2 = it->elem; }
        else if (it->kind == TK::Map) { t1 = it->key; t2 = it->elem; }
        else if (it->kind == TK::Str) { t1 = two ? T.intT : T.strT; t2 = T.strT; }
        else if (!it->isError()) {
            Diag& d = err(n->kids[0]->loc, "`" + it->str() + "` is not iterable", "cannot loop over this");
            if (it->kind == TK::Int) d.notes.push_back("help: to loop over numbers use a range: `for i in 0..n`");
        }
        pushScope();
        n->var = declare(n->names[0], t1, false, n->nameLocs[0]);
        if (two) n->var2 = declare(n->names[1], t2, false, n->nameLocs[1]);
        loopDepth_++;
        block(n->kids[1], false);
        loopDepth_--;
        popScope();
        return;
    }
    case NK::Break: case NK::Continue:
        if (loopDepth_ == 0) err(n->loc, "`" + std::string(n->kind == NK::Break ? "break" : "continue") + "` outside of a loop");
        return;
    case NK::Return: {
        if (!fn_) { err(n->loc, "`return` outside of a function"); return; }
        if (n->kids.empty()) {
            if (fn_->retFromBody && !fn_->ret) fn_->ret = T.voidT;
            else if (fn_->ret && fn_->ret->kind != TK::Void && !fn_->ret->isError()) {
                Diag& d = err(n->loc, "`return` without a value in a function returning `" + fn_->ret->str() + "`");
                d.notes.push_back("help: return a value: `return x`");
            }
            return;
        }
        Type* t = expr(n->kids[0], fn_->ret);
        if (fn_->retFromBody && !fn_->ret) {
            if (t->kind == TK::Nil) {
                Diag& d = err(n->kids[0]->loc, "cannot infer the return type from `nil`");
                d.notes.push_back("help: declare the return type: `fn " + fn_->name + "(...) -> ?int`");
                fn_->ret = T.errorT;
            } else if (t->kind == TK::Void) {
                err(n->kids[0]->loc, "this expression does not produce a value");
                fn_->ret = T.errorT;
            } else {
                fn_->ret = t;
            }
            return;
        }
        if (fn_->ret->kind == TK::Void) {
            Diag& d = err(n->kids[0]->loc, "function `" + fn_->displayName() + "` has no return type but returns a value");
            if (!t->isError() && t->kind != TK::Void)
                d.notes.push_back("help: declare the return type: `-> " + t->str() + "`");
            return;
        }
        coerce(n->kids[0], fn_->ret, "");
        return;
    }
    case NK::Block:
        block(n);
        return;
    default:
        err(n->loc, "internal error: unexpected statement");
    }
}

// ------------------------------------------------------------ expressions --

bool Checker::coerce(Node*& n, Type* target, const std::string& what) {
    Type* t = n->type;
    if (!t || !target || t->isError() || target->isError()) return true;
    if (t == target) return true;
    if (target->kind == TK::Float && n->kind == NK::IntLit) {
        n->kind = NK::FloatLit;
        n->fval = (double)n->ival;
        n->type = target;
        return true;
    }
    if (target->kind == TK::Optional) {
        if (t->kind == TK::Nil) { n->type = target; return true; }
        if (target->elem->kind == TK::Float && n->kind == NK::IntLit) {
            n->kind = NK::FloatLit;
            n->fval = (double)n->ival;
            n->type = T.floatT;
            t = T.floatT;
        }
        if (t == target->elem) {
            Node* w = new Node(NK::Wrap, n->loc);
            w->kids.push_back(n);
            w->type = target;
            n = w;
            return true;
        }
    }
    mismatch(n->loc, target, t, what);
    return false;
}

static bool isIntLit(Node* n) { return n->kind == NK::IntLit; }

Type* Checker::unify(Node*& a, Node*& b) {
    Type* ta = a->type;
    Type* tb = b->type;
    if (ta->kind == TK::Float && tb->kind == TK::Int && isIntLit(b)) coerce(b, T.floatT);
    else if (tb->kind == TK::Float && ta->kind == TK::Int && isIntLit(a)) coerce(a, T.floatT);
    return a->type;
}

bool Checker::isComparable(Type* t) {
    return t->kind != TK::Void && t->kind != TK::Nil;
}

bool Checker::isPrintable(Type* t, const Loc& loc) {
    if (t->isError()) return false;
    if (t->kind == TK::Void) { err(loc, "this expression does not produce a value", "returns nothing"); return false; }
    if (t->kind == TK::Nil) { err(loc, "cannot infer the type of `nil` here"); return false; }
    return true;
}

void Checker::checkFormatSpec(const std::string& spec, Type* t, const Loc& loc) {
    if (spec.empty()) return;
    size_t i = 0;
    auto isAlign = [](char c) { return c == '<' || c == '>' || c == '^'; };
    if (spec.size() >= 2 && isAlign(spec[1])) i = 2;
    else if (isAlign(spec[0])) i = 1;
    while (i < spec.size() && std::isdigit((unsigned char)spec[i])) i++;
    if (i < spec.size() && spec[i] == '.') {
        size_t s = ++i;
        while (i < spec.size() && std::isdigit((unsigned char)spec[i])) i++;
        if (i == s) { err(loc, "invalid format spec `:" + spec + "`", "missing precision digits"); return; }
        if (t->kind != TK::Float && !t->isError()) {
            err(loc, "precision `." + spec.substr(s, i - s) + "` only applies to `float` values, found `" + t->str() + "`");
            return;
        }
    }
    if (i != spec.size()) {
        Diag& d = err(loc, "invalid format spec `:" + spec + "`");
        d.notes.push_back("help: format specs look like `{x:8}`, `{x:<8}`, `{x:^8}`, `{x:08}` or `{x:.2}`");
    }
}

Type* Checker::expr(Node* n, Type* expected) {
    Type* t = T.errorT;
    switch (n->kind) {
    case NK::IntLit: t = T.intT; break;
    case NK::FloatLit: t = T.floatT; break;
    case NK::BoolLit: t = T.boolT; break;
    case NK::StrLit: t = T.strT; break;
    case NK::NilLit:
        t = (expected && expected->kind == TK::Optional) ? expected : T.nilT;
        break;
    case NK::FStr:
        for (size_t i = 0; i < n->kids.size(); i++) {
            Type* pt = expr(n->kids[i]);
            if (isPrintable(pt, n->kids[i]->loc)) checkFormatSpec(n->specs[i], pt, n->nameLocs[i]);
        }
        t = T.strT;
        break;
    case NK::Ident: {
        Var* v = lookup(n->str);
        if (!v) {
            if (prog_.fnByName.count(n->str)) {
                Diag& d = err(n->loc, "`" + n->str + "` is a function, not a value");
                d.notes.push_back("help: call it: `" + n->str + "()`");
            } else if (n->str == "self") {
                err(n->loc, "`self` is only available inside methods that take `self`");
            } else if (n->str == "True" || n->str == "False" || n->str == "None" || n->str == "null" || n->str == "NULL") {
                Diag& d = err(n->loc, "cannot find value `" + n->str + "` in this scope", "not found");
                d.notes.push_back(std::string("help: Arc spells it `") +
                                  (n->str == "True" ? "true" : n->str == "False" ? "false" : "nil") + "`");
            } else {
                Diag& d = err(n->loc, "cannot find value `" + n->str + "` in this scope", "not found in this scope");
                std::string s = suggest(n->str, visibleNames());
                if (!s.empty()) d.notes.push_back("help: a variable with a similar name exists: `" + s + "`");
            }
            break;
        }
        v->used = true;
        n->var = v;
        t = v->type;
        break;
    }
    case NK::Unary: {
        Type* o = expr(n->kids[0], expected);
        if (o->isError()) break;
        if (n->str == "-") {
            if (o->isNumeric()) t = o;
            else err(n->loc, "cannot negate a value of type `" + o->str() + "`");
        } else if (n->str == "!") {
            if (o->kind == TK::Bool) t = o;
            else {
                Diag& d = err(n->loc, "cannot apply `!` to `" + o->str() + "`");
                if (o->kind == TK::Int) d.notes.push_back("help: for bitwise NOT use `~x`; for a test use `x == 0`");
                if (o->kind == TK::Optional) d.notes.push_back("help: test for nil with `x == nil`");
            }
        } else if (n->str == "~") {
            if (o->kind == TK::Int) t = o;
            else err(n->loc, "cannot apply `~` to `" + o->str() + "`");
        }
        break;
    }
    case NK::Binary: t = binary(n); break;
    case NK::Call: t = call(n); break;
    case NK::MethodCall: t = methodCall(n); break;
    case NK::PathCall: t = pathCall(n); break;
    case NK::Field: {
        Type* o = expr(n->kids[0]);
        if (o->isError()) break;
        if (o->kind == TK::Struct) {
            int idx = o->sd->fieldIndex(n->str);
            if (idx >= 0) { t = o->sd->fields[idx].type; n->ival = idx; break; }
            Loc fl = n->nameLocs.empty() ? n->loc : n->nameLocs[0];
            if (o->sd->methods.count(n->str)) {
                Diag& d = err(fl, "`" + n->str + "` is a method of `" + o->str() + "`, not a field");
                d.notes.push_back("help: call it: `." + n->str + "()`");
                break;
            }
            std::vector<std::string> names;
            for (auto& f : o->sd->fields) names.push_back(f.name);
            Diag& d = err(fl, "no field `" + n->str + "` on type `" + o->str() + "`", "unknown field");
            std::string s = suggest(n->str, names);
            if (!s.empty()) d.notes.push_back("help: a field with a similar name exists: `" + s + "`");
            else if (!names.empty()) {
                std::string all;
                for (auto& f : names) all += (all.empty() ? "" : ", ") + std::string("`") + f + "`";
                d.notes.push_back("note: available fields are: " + all);
            }
            break;
        }
        Loc fl = n->nameLocs.empty() ? n->loc : n->nameLocs[0];
        Diag& d = err(fl, "type `" + o->str() + "` has no field `" + n->str + "`");
        if ((o->kind == TK::Str || o->kind == TK::Array || o->kind == TK::Map) && (n->str == "len" || n->str == "length" || n->str == "size"))
            d.notes.push_back("help: use the method: `.len()`");
        if (o->kind == TK::Optional && o->elem->kind == TK::Struct)
            d.notes.push_back("help: the value may be nil; unwrap it first: `if let v = x { v." + n->str + " }`");
        break;
    }
    case NK::Index: {
        Type* o = expr(n->kids[0]);
        if (o->kind == TK::Map) {
            Type* k = expr(n->kids[1], o->key);
            coerce(n->kids[1], o->key, "map key");
            (void)k;
            t = o->elem;
            break;
        }
        Type* i = expr(n->kids[1], T.intT);
        if (o->isError()) break;
        if (o->kind == TK::Array) {
            if (i->kind != TK::Int && !i->isError()) mismatch(n->kids[1]->loc, T.intT, i, "array index must be `int`");
            t = o->elem;
        } else if (o->kind == TK::Str) {
            Diag& d = err(n->loc, "strings cannot be indexed by position");
            d.notes.push_back("help: use `s.byte_at(i)` for a byte, `s.chars()[i]` for a character, or a slice `s[a..b]`");
        } else {
            err(n->loc, "cannot index into a value of type `" + o->str() + "`");
        }
        break;
    }
    case NK::Slice: {
        Type* o = expr(n->kids[0]);
        for (int k = 1; k <= 2; k++) {
            if (!n->kids[k]) continue;
            Type* b = expr(n->kids[k], T.intT);
            if (b->kind != TK::Int && !b->isError()) mismatch(n->kids[k]->loc, T.intT, b, "slice bounds must be `int`");
        }
        if (o->isError()) break;
        if (o->kind == TK::Array || o->kind == TK::Str) t = o;
        else err(n->loc, "cannot slice a value of type `" + o->str() + "`");
        break;
    }
    case NK::ArrayLit: {
        Type* et = expected && expected->kind == TK::Array ? expected->elem : nullptr;
        if (n->kids.empty()) {
            if (et) { t = expected; break; }
            Diag& d = err(n->loc, "cannot infer the element type of an empty array");
            d.notes.push_back("help: add a type annotation: `let a: [int] = []`");
            break;
        }
        bool bad = false, anyNil = false, anyFloat = false;
        for (Node* k : n->kids) {
            Type* kt = expr(k, et);
            if (kt->isError()) bad = true;
            if (kt->kind == TK::Nil) anyNil = true;
            if (kt->kind == TK::Float) anyFloat = true;
        }
        if (bad) break;
        if (!et) {
            for (Node* k : n->kids) if (k->type->kind != TK::Nil) { et = k->type; break; }
            if (!et) { err(n->loc, "cannot infer the element type of an array of `nil`"); break; }
            if (et->kind == TK::Int && anyFloat) et = T.floatT;
            if (anyNil && et->kind != TK::Optional) et = T.optional(et);
        }
        for (Node*& k : n->kids) coerce(k, et, "array element");
        t = T.array(et);
        break;
    }
    case NK::ArrayRepeat: {
        Type* et = expected && expected->kind == TK::Array ? expected->elem : nullptr;
        Type* v = expr(n->kids[0], et);
        Type* c = expr(n->kids[1], T.intT);
        if (c->kind != TK::Int && !c->isError()) mismatch(n->kids[1]->loc, T.intT, c, "repeat count must be `int`");
        if (v->isError()) break;
        if (et) coerce(n->kids[0], et); else et = v;
        if (et->kind == TK::Nil) { err(n->loc, "cannot infer the element type of an array of `nil`"); break; }
        if (et->kind == TK::Void) { err(n->kids[0]->loc, "this expression does not produce a value"); break; }
        t = T.array(et);
        break;
    }
    case NK::MapLit: {
        Type* kt = expected && expected->kind == TK::Map ? expected->key : nullptr;
        Type* vt = expected && expected->kind == TK::Map ? expected->elem : nullptr;
        if (n->kids.empty()) {
            if (kt) { t = expected; break; }
            Diag& d = err(n->loc, "cannot infer the key and value types of an empty map");
            d.notes.push_back("help: add a type annotation: `let m: [str: int] = [:]`");
            break;
        }
        bool bad = false;
        for (size_t i = 0; i < n->kids.size(); i += 2) {
            if (expr(n->kids[i], kt)->isError()) bad = true;
            if (expr(n->kids[i + 1], vt)->isError()) bad = true;
        }
        if (bad) break;
        if (!kt) kt = n->kids[0]->type;
        if (!vt) {
            vt = n->kids[1]->type;
            for (size_t i = 1; i < n->kids.size(); i += 2)
                if (n->kids[i]->type->kind == TK::Float && vt->kind == TK::Int) vt = T.floatT;
        }
        if (kt->kind != TK::Int && kt->kind != TK::Str && kt->kind != TK::Bool) {
            err(n->kids[0]->loc, "map keys must be `int`, `str` or `bool`, not `" + kt->str() + "`");
            break;
        }
        for (size_t i = 0; i < n->kids.size(); i += 2) {
            coerce(n->kids[i], kt, "map key");
            coerce(n->kids[i + 1], vt, "map value");
        }
        t = T.map(kt, vt);
        break;
    }
    case NK::StructLit: {
        auto it = prog_.structByName.find(n->str);
        if (it == prog_.structByName.end()) {
            std::vector<std::string> names;
            for (auto& kv : prog_.structByName) names.push_back(kv.first);
            Diag& d = err(n->loc, "cannot find struct `" + n->str + "`", "not found");
            std::string s = suggest(n->str, names);
            if (!s.empty()) d.notes.push_back("help: did you mean `" + s + "`?");
            for (Node* k : n->kids) expr(k);
            break;
        }
        StructDecl* sd = it->second;
        std::vector<int> seen(sd->fields.size(), 0);
        bool ok = true;
        for (size_t i = 0; i < n->names.size(); i++) {
            int idx = sd->fieldIndex(n->names[i]);
            if (idx < 0) {
                std::vector<std::string> names;
                for (auto& f : sd->fields) names.push_back(f.name);
                Diag& d = err(n->nameLocs[i], "struct `" + sd->name + "` has no field named `" + n->names[i] + "`");
                std::string s = suggest(n->names[i], names);
                if (!s.empty()) d.notes.push_back("help: did you mean `" + s + "`?");
                expr(n->kids[i]);
                ok = false;
                continue;
            }
            if (seen[idx]++) { err(n->nameLocs[i], "field `" + n->names[i] + "` specified more than once"); ok = false; }
            expr(n->kids[i], sd->fields[idx].type);
            coerce(n->kids[i], sd->fields[idx].type, "");
        }
        std::string missing;
        for (size_t i = 0; i < sd->fields.size(); i++)
            if (!seen[i]) missing += (missing.empty() ? "`" : ", `") + sd->fields[i].name + "`";
        if (!missing.empty()) {
            err(n->loc, "missing field" + std::string(missing.find(',') != std::string::npos ? "s " : " ") + missing +
                            " in initializer of `" + sd->name + "`");
            ok = false;
        }
        (void)ok;
        t = sd->type;
        break;
    }
    case NK::Cast: {
        Type* from = expr(n->kids[0]);
        Type* to = resolveType(n->tyExpr);
        if (from->isError() || to->isError()) break;
        bool ok = (from->isNumeric() && to->isNumeric()) || (from->kind == TK::Bool && to->kind == TK::Int) || from == to;
        if (!ok) {
            Diag& d = err(n->loc, "cannot cast `" + from->str() + "` as `" + to->str() + "`");
            if (to->kind == TK::Str) d.notes.push_back("help: use `str(x)` to convert to a string");
            if (from->kind == TK::Str && to->kind == TK::Int) d.notes.push_back("help: use `parse_int(s)`, which returns `?int`");
            if (from->kind == TK::Str && to->kind == TK::Float) d.notes.push_back("help: use `parse_float(s)`, which returns `?float`");
            break;
        }
        t = to;
        break;
    }
    case NK::Wrap: t = n->type; break;
    default:
        err(n->loc, "internal error: unexpected expression");
    }
    n->type = t;
    return t;
}

Type* Checker::binary(Node* n) {
    const std::string& op = n->str;
    if (op == "&&" || op == "||") {
        Type* a = expr(n->kids[0], T.boolT);
        Type* b = expr(n->kids[1], T.boolT);
        if (a->kind != TK::Bool && !a->isError()) mismatch(n->kids[0]->loc, T.boolT, a, "expected `bool` operand");
        if (b->kind != TK::Bool && !b->isError()) mismatch(n->kids[1]->loc, T.boolT, b, "expected `bool` operand");
        return T.boolT;
    }
    if (op == "??") {
        Type* a = expr(n->kids[0]);
        if (a->isError()) { expr(n->kids[1]); return T.errorT; }
        if (a->kind != TK::Optional) {
            Diag& d = err(n->kids[0]->loc, "the left side of `??` must be optional, found `" + a->str() + "`");
            d.notes.push_back("help: `??` provides a default for a `nil` value: `maybe ?? 0`");
            expr(n->kids[1]);
            return T.errorT;
        }
        Type* b = expr(n->kids[1], a->elem);
        if (b == a) return a;
        coerce(n->kids[1], a->elem, "");
        return a->elem;
    }
    Type* a = expr(n->kids[0]);
    Type* b = expr(n->kids[1], a->kind == TK::Optional && n->kids[1]->kind == NK::NilLit ? a : (a->isNumeric() ? a : nullptr));
    if (a->isError() || b->isError()) return (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") ? T.boolT : T.errorT;
    if (n->kids[0]->kind == NK::NilLit && b->kind == TK::Optional) { n->kids[0]->type = b; a = b; }
    unify(n->kids[0], n->kids[1]);
    a = n->kids[0]->type;
    b = n->kids[1]->type;

    if (op == "==" || op == "!=") {
        if (a == b && isComparable(a)) return T.boolT;
        if (a->kind == TK::Optional && b == a->elem) { coerce(n->kids[1], a); return T.boolT; }
        if (b->kind == TK::Optional && a == b->elem) { coerce(n->kids[0], b); return T.boolT; }
        if (a->kind == TK::Nil && b->kind == TK::Nil) {
            err(n->loc, "comparing `nil` with `nil` is always true");
            return T.boolT;
        }
        Diag& d = err(n->loc, "cannot compare `" + a->str() + "` with `" + b->str() + "`");
        if ((a->kind == TK::Nil) || (b->kind == TK::Nil))
            d.notes.push_back("help: only optional values (`?T`) can be compared with `nil`");
        else if (a->isNumeric() && b->isNumeric())
            d.notes.push_back("help: convert one side so both have the same type, e.g. `float(x)`");
        return T.boolT;
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (a == b && (a->isNumeric() || a->kind == TK::Str)) return T.boolT;
        Diag& d = err(n->loc, "cannot compare `" + a->str() + "` and `" + b->str() + "` with `" + op + "`");
        if (a->isNumeric() && b->isNumeric()) d.notes.push_back("help: convert one side so both have the same type, e.g. `float(x)`");
        return T.boolT;
    }
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        if (a == b && a->isNumeric()) return a;
        if (op == "+" && a->kind == TK::Str && b->kind == TK::Str) return a;
        Diag& d = err(n->loc, "cannot apply `" + op + "` to `" + a->str() + "` and `" + b->str() + "`");
        if (op == "+" && (a->kind == TK::Str || b->kind == TK::Str))
            d.notes.push_back("help: convert with `str(x)` or use an f-string: f\"...{x}...\"");
        else if (a->isNumeric() && b->isNumeric())
            d.notes.push_back("help: Arc never mixes `int` and `float` implicitly; convert with `float(x)` or `int(x)`");
        else if (a->kind == TK::Optional || b->kind == TK::Optional)
            d.notes.push_back("help: unwrap the optional first: `x ?? 0` or `x.unwrap()`");
        return T.errorT;
    }
    if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
        if (a->kind == TK::Int && b->kind == TK::Int) return a;
        Diag& d = err(n->loc, "cannot apply `" + op + "` to `" + a->str() + "` and `" + b->str() + "`");
        if (a->kind == TK::Bool) d.notes.push_back(std::string("help: for logic use `") + (op == "&" ? "&&" : "||") + "`");
        return T.errorT;
    }
    err(n->loc, "unknown operator `" + op + "`");
    return T.errorT;
}

// `action` may contain `{}`, replaced by the target's source text.
void Checker::requireMutable(Node* target, const Loc& at, const std::string& action) {
    Node* root = target;
    bool direct = true;
    while (root->kind == NK::Field || root->kind == NK::Index || root->kind == NK::Slice) {
        root = root->kids[0];
        direct = false;
    }
    if (root->kind != NK::Ident || !root->var) return;
    Var* v = root->var;
    v->assigned = true;
    if (v->mut) return;
    std::string text = "`" + srcText(target->loc) + "`";
    std::string msg = action;
    size_t ph = msg.find("{}");
    if (ph != std::string::npos) msg.replace(ph, 2, text);
    else msg += " " + text;
    if (v->name == "self") {
        Diag& d = err(at, "cannot " + msg + ", as `self` is not mutable");
        d.notes.push_back("help: declare the method with a mutable receiver: `fn " + (fn_ ? fn_->name : std::string("f")) + "(mut self, ...)`");
        return;
    }
    Diag& d = direct && action == "assign to"
                  ? err(at, "cannot assign twice to immutable variable `" + v->name + "`", "cannot assign twice")
                  : err(at, "cannot " + msg + ", as `" + v->name + "` is not declared as mutable", "cannot mutate");
    if (v->isGlobal) d.notes.push_back("help: make the global mutable: `let mut " + v->name + " = ...`");
    else if (v->isParam) d.notes.push_back("help: make the parameter mutable: `mut " + v->name + ": " + v->type->str() + "`");
    else d.notes.push_back("help: consider making this binding mutable: `let mut " + v->name + "` (declared at line " + std::to_string(v->loc.line) + ")");
}

std::string Checker::srcText(const Loc& l) {
    if (l.file < 0 || l.file >= (int)prog_.files.size()) return "?";
    const std::string& t = prog_.files[l.file]->text;
    if (l.off >= t.size()) return "?";
    std::string s = t.substr(l.off, l.len);
    size_t nl = s.find('\n');
    if (nl != std::string::npos) s = s.substr(0, nl);
    return s;
}

void Checker::checkArgs(Node* n, size_t firstArg, FnDecl* fn, size_t firstParam) {
    size_t nargs = n->kids.size() - firstArg;
    size_t nparams = fn->params.size() - firstParam;
    if (nargs != nparams) {
        Diag& d = err(n->loc, "`" + fn->displayName() + "` takes " + std::to_string(nparams) + " argument" +
                                  (nparams == 1 ? "" : "s") + " but " + std::to_string(nargs) + (nargs == 1 ? " was" : " were") + " supplied");
        std::string sig = "fn " + fn->displayName() + "(";
        for (size_t i = firstParam; i < fn->params.size(); i++) {
            if (i > firstParam) sig += ", ";
            sig += fn->params[i].name + ": " + fn->paramTypes[i]->str();
        }
        sig += ")";
        d.notes.push_back("note: defined at line " + std::to_string(fn->loc.line) + " as `" + sig + "`");
        for (size_t i = firstArg; i < n->kids.size(); i++) expr(n->kids[i]);
        return;
    }
    for (size_t i = 0; i < nargs; i++) {
        Type* pt = fn->paramTypes[firstParam + i];
        const Param& prm = fn->params[firstParam + i];
        expr(n->kids[firstArg + i], pt);
        coerce(n->kids[firstArg + i], pt, "argument `" + prm.name + "` expects `" + pt->str() + "`");
        // passing a heap value to a `mut` parameter lets the callee modify it
        if (prm.mut && pt->isHeap() && pt->kind != TK::Str)
            requireMutable(n->kids[firstArg + i], n->kids[firstArg + i]->loc,
                           "pass {} to mutable parameter `" + prm.name + "`");
    }
}

Type* Checker::call(Node* n) {
    auto it = prog_.fnByName.find(n->str);
    if (it != prog_.fnByName.end()) {
        FnDecl* fn = it->second;
        n->fn = fn;
        checkArgs(n, 0, fn, 0);
        return ensureReturnType(fn, n->loc);
    }
    if (std::find(kBuiltinFns.begin(), kBuiltinFns.end(), n->str) != kBuiltinFns.end()) return builtinCall(n);
    for (Node* k : n->kids) expr(k);
    if (prog_.structByName.count(n->str)) {
        Diag& d = err(n->loc, "`" + n->str + "` is a struct, not a function");
        d.notes.push_back("help: create it with a struct literal: `" + n->str + " { field: value }`");
        return T.errorT;
    }
    std::vector<std::string> names = kBuiltinFns;
    for (auto& kv : prog_.fnByName) names.push_back(kv.first);
    Diag& d = err(n->loc, "cannot find function `" + n->str + "` in this scope", "not found");
    std::string s = suggest(n->str, names);
    if (!s.empty()) d.notes.push_back("help: a function with a similar name exists: `" + s + "`");
    else if (n->str == "printf" || n->str == "puts" || n->str == "echo" || n->str == "console")
        d.notes.push_back("help: use `println(...)`");
    else if (n->str == "malloc" || n->str == "free" || n->str == "peek" || n->str == "poke")
        d.notes.push_back("note: Arc manages memory automatically; use arrays, maps and structs");
    return T.errorT;
}

Type* Checker::pathCall(Node* n) {
    const std::string& tname = n->names[0];
    auto it = prog_.structByName.find(tname);
    if (it == prog_.structByName.end()) {
        err(n->loc, "cannot find struct `" + tname + "`", "not found");
        for (Node* k : n->kids) expr(k);
        return T.errorT;
    }
    StructDecl* sd = it->second;
    auto mit = sd->methods.find(n->str);
    if (mit == sd->methods.end()) {
        std::vector<std::string> names;
        for (auto& kv : sd->methods) names.push_back(kv.first);
        Diag& d = err(n->loc, "no function named `" + n->str + "` found for struct `" + tname + "`");
        std::string s = suggest(n->str, names);
        if (!s.empty()) d.notes.push_back("help: did you mean `" + tname + "::" + s + "`?");
        for (Node* k : n->kids) expr(k);
        return T.errorT;
    }
    FnDecl* fn = mit->second;
    n->fn = fn;
    if (fn->hasSelf) {
        // Type::method(receiver, args...)
        if (n->kids.empty()) {
            err(n->loc, "`" + fn->displayName() + "` is a method; pass the receiver as the first argument or call `x." + fn->name + "()`");
            return T.errorT;
        }
        expr(n->kids[0], sd->type);
        coerce(n->kids[0], sd->type, "receiver");
        if (fn->selfMut) requireMutable(n->kids[0], n->kids[0]->loc, "call mutating method `" + fn->name + "` on {}");
        checkArgs(n, 1, fn, 0);
    } else {
        checkArgs(n, 0, fn, 0);
    }
    return ensureReturnType(fn, n->loc);
}

Type* Checker::methodCall(Node* n) {
    Type* recv = expr(n->kids[0]);
    if (recv->isError()) {
        for (size_t i = 1; i < n->kids.size(); i++) expr(n->kids[i]);
        return T.errorT;
    }
    if (recv->kind == TK::Struct) {
        StructDecl* sd = recv->sd;
        auto it = sd->methods.find(n->str);
        if (it == sd->methods.end()) {
            for (size_t i = 1; i < n->kids.size(); i++) expr(n->kids[i]);
            if (sd->fieldIndex(n->str) >= 0) {
                Diag& d = err(n->loc, "`" + n->str + "` is a field of `" + sd->name + "`, not a method");
                d.notes.push_back("help: remove the parentheses: `." + n->str + "`");
                return T.errorT;
            }
            std::vector<std::string> names;
            for (auto& kv : sd->methods) names.push_back(kv.first);
            Diag& d = err(n->loc, "no method named `" + n->str + "` found for struct `" + sd->name + "`", "method not found");
            std::string s = suggest(n->str, names);
            if (!s.empty()) d.notes.push_back("help: did you mean `" + s + "`?");
            else d.notes.push_back("help: add it in an `impl " + sd->name + " { fn " + n->str + "(self) { ... } }` block");
            return T.errorT;
        }
        FnDecl* fn = it->second;
        n->fn = fn;
        if (!fn->hasSelf) {
            Diag& d = err(n->loc, "`" + fn->displayName() + "` is an associated function, not a method");
            d.notes.push_back("help: call it as `" + sd->name + "::" + fn->name + "(...)`");
            return T.errorT;
        }
        if (fn->selfMut) requireMutable(n->kids[0], n->kids[0]->loc, "call mutating method `" + fn->name + "` on {}");
        checkArgs(n, 1, fn, 0);
        return ensureReturnType(fn, n->loc);
    }
    return builtinMethod(n, recv);
}

bool Checker::expectArgs(Node* n, size_t firstArg, size_t count, const std::string& sig) {
    size_t got = n->kids.size() - firstArg;
    if (got == count) return true;
    Diag& d = err(n->loc, "`" + n->str + "` takes " + std::to_string(count) + " argument" + (count == 1 ? "" : "s") +
                              " but " + std::to_string(got) + (got == 1 ? " was" : " were") + " supplied");
    d.notes.push_back("note: signature: `" + sig + "`");
    for (size_t i = firstArg; i < n->kids.size(); i++) expr(n->kids[i]);
    return false;
}

Type* Checker::builtinCall(Node* n) {
    const std::string& f = n->str;
    auto argT = [&](size_t i, Type* want, const std::string& what = "") -> Type* {
        Type* t = expr(n->kids[i], want);
        if (want) { coerce(n->kids[i], want, what); return n->kids[i]->type; }
        return t;
    };
    auto setB = [&](BI b) { n->builtin = b; };

    if (f == "print" || f == "println" || f == "eprint" || f == "eprintln") {
        setB(f == "print" ? BI::Print : f == "println" ? BI::Println : f == "eprint" ? BI::Eprint : BI::Eprintln);
        for (Node* k : n->kids) isPrintable(expr(k), k->loc);
        return T.voidT;
    }
    if (f == "str") {
        setB(BI::Str);
        if (!expectArgs(n, 0, 1, "str(value) -> str")) return T.strT;
        isPrintable(expr(n->kids[0]), n->kids[0]->loc);
        return T.strT;
    }
    if (f == "int" || f == "float") {
        bool toInt = f == "int";
        setB(toInt ? BI::Int : BI::Float);
        if (!expectArgs(n, 0, 1, f + "(x) -> " + f)) return toInt ? T.intT : T.floatT;
        Type* t = expr(n->kids[0]);
        if (t->isError()) return toInt ? T.intT : T.floatT;
        bool ok = t->isNumeric() || (toInt && t->kind == TK::Bool);
        if (!ok) {
            Diag& d = err(n->kids[0]->loc, "cannot convert `" + t->str() + "` to `" + f + "`");
            if (t->kind == TK::Str) d.notes.push_back("help: use `parse_" + f + "(s)`, which returns `?" + f + "`");
        }
        return toInt ? T.intT : T.floatT;
    }
    if (f == "len") {
        setB(BI::Len);
        if (!expectArgs(n, 0, 1, "len(x) -> int")) return T.intT;
        Type* t = expr(n->kids[0]);
        if (!t->isError() && t->kind != TK::Str && t->kind != TK::Array && t->kind != TK::Map)
            err(n->kids[0]->loc, "`len` expects a string, array or map, found `" + t->str() + "`");
        return T.intT;
    }
    if (f == "parse_int" || f == "parse_float") {
        setB(f == "parse_int" ? BI::ParseInt : BI::ParseFloat);
        if (!expectArgs(n, 0, 1, f + "(s: str) -> ?" + f.substr(6))) return T.errorT;
        argT(0, T.strT);
        return T.optional(f == "parse_int" ? T.intT : T.floatT);
    }
    if (f == "abs") {
        setB(BI::Abs);
        if (!expectArgs(n, 0, 1, "abs(x) -> int|float")) return T.errorT;
        Type* t = expr(n->kids[0]);
        if (t->isError()) return t;
        if (!t->isNumeric()) { err(n->kids[0]->loc, "`abs` expects `int` or `float`, found `" + t->str() + "`"); return T.errorT; }
        return t;
    }
    if (f == "min" || f == "max") {
        setB(f == "min" ? BI::Min : BI::Max);
        if (!expectArgs(n, 0, 2, f + "(a, b)")) return T.errorT;
        Type* a = expr(n->kids[0]);
        Type* b = expr(n->kids[1], a->isNumeric() ? a : nullptr);
        if (a->isError() || b->isError()) return T.errorT;
        unify(n->kids[0], n->kids[1]);
        a = n->kids[0]->type;
        b = n->kids[1]->type;
        if (a != b || !(a->isNumeric() || a->kind == TK::Str)) {
            err(n->loc, "`" + f + "` expects two values of the same numeric type, found `" + a->str() + "` and `" + b->str() + "`");
            return T.errorT;
        }
        return a;
    }
    static const std::map<std::string, BI> floatFns1 = {
        {"sqrt", BI::Sqrt}, {"floor", BI::Floor}, {"ceil", BI::Ceil}, {"round", BI::Round}, {"sin", BI::Sin},
        {"cos", BI::Cos}, {"tan", BI::Tan}, {"exp", BI::Exp}, {"log", BI::Log}, {"log10", BI::Log10}};
    auto ff = floatFns1.find(f);
    if (ff != floatFns1.end()) {
        setB(ff->second);
        if (!expectArgs(n, 0, 1, f + "(x: float) -> float")) return T.floatT;
        argT(0, T.floatT, "`" + f + "` expects `float`");
        return T.floatT;
    }
    if (f == "pow" || f == "atan2") {
        setB(f == "pow" ? BI::Pow : BI::Atan2);
        if (!expectArgs(n, 0, 2, f + "(a: float, b: float) -> float")) return T.floatT;
        argT(0, T.floatT);
        argT(1, T.floatT);
        return T.floatT;
    }
    if (f == "ipow") {
        setB(BI::IPow);
        if (!expectArgs(n, 0, 2, "ipow(base: int, exp: int) -> int")) return T.intT;
        argT(0, T.intT);
        argT(1, T.intT);
        return T.intT;
    }
    if (f == "assert") {
        setB(BI::Assert);
        if (n->kids.size() != 1 && n->kids.size() != 2) { expectArgs(n, 0, 1, "assert(cond: bool, message: str = \"\")"); return T.voidT; }
        argT(0, T.boolT, "assertion condition");
        if (n->kids.size() == 2) argT(1, T.strT);
        return T.voidT;
    }
    if (f == "assert_eq") {
        setB(BI::AssertEq);
        if (!expectArgs(n, 0, 2, "assert_eq(left, right)")) return T.voidT;
        Type* a = expr(n->kids[0]);
        Type* b = expr(n->kids[1], a);
        if (a->isError() || b->isError()) return T.voidT;
        unify(n->kids[0], n->kids[1]);
        a = n->kids[0]->type;
        if (n->kids[1]->type != a) coerce(n->kids[1], a);
        isPrintable(a, n->kids[0]->loc);
        return T.voidT;
    }
    if (f == "panic") {
        setB(BI::Panic);
        if (n->kids.size() > 1) { expectArgs(n, 0, 1, "panic(message: str)"); return T.voidT; }
        if (n->kids.size() == 1) argT(0, T.strT);
        return T.voidT;
    }
    if (f == "exit") {
        setB(BI::Exit);
        if (!expectArgs(n, 0, 1, "exit(code: int)")) return T.voidT;
        argT(0, T.intT);
        return T.voidT;
    }
    if (f == "read_line") { setB(BI::ReadLine); expectArgs(n, 0, 0, "read_line() -> ?str"); return T.optional(T.strT); }
    if (f == "read_file") {
        setB(BI::ReadFile);
        if (expectArgs(n, 0, 1, "read_file(path: str) -> ?str")) argT(0, T.strT);
        return T.optional(T.strT);
    }
    if (f == "write_file" || f == "append_file") {
        setB(f == "write_file" ? BI::WriteFile : BI::AppendFile);
        if (expectArgs(n, 0, 2, f + "(path: str, data: str) -> bool")) { argT(0, T.strT); argT(1, T.strT); }
        return T.boolT;
    }
    if (f == "file_exists") {
        setB(BI::FileExists);
        if (expectArgs(n, 0, 1, "file_exists(path: str) -> bool")) argT(0, T.strT);
        return T.boolT;
    }
    if (f == "getenv") {
        setB(BI::Getenv);
        if (expectArgs(n, 0, 1, "getenv(name: str) -> ?str")) argT(0, T.strT);
        return T.optional(T.strT);
    }
    if (f == "run") {
        setB(BI::RunCmd);
        if (expectArgs(n, 0, 1, "run(command: str) -> int")) argT(0, T.strT);
        return T.intT;
    }
    if (f == "args") { setB(BI::Args); expectArgs(n, 0, 0, "args() -> [str]"); return T.array(T.strT); }
    if (f == "time_ns") { setB(BI::TimeNs); expectArgs(n, 0, 0, "time_ns() -> int"); return T.intT; }
    if (f == "time_ms") { setB(BI::TimeMs); expectArgs(n, 0, 0, "time_ms() -> int"); return T.intT; }
    if (f == "unix_time") { setB(BI::UnixTime); expectArgs(n, 0, 0, "unix_time() -> int"); return T.intT; }
    if (f == "gc_count") { setB(BI::GcCount); expectArgs(n, 0, 0, "gc_count() -> int"); return T.intT; }
    if (f == "sleep_ms") {
        setB(BI::SleepMs);
        if (expectArgs(n, 0, 1, "sleep_ms(ms: int)")) argT(0, T.intT);
        return T.voidT;
    }
    if (f == "rand_int") {
        setB(BI::RandInt);
        if (expectArgs(n, 0, 2, "rand_int(lo: int, hi: int) -> int")) { argT(0, T.intT); argT(1, T.intT); }
        return T.intT;
    }
    if (f == "rand_float") { setB(BI::RandFloat); expectArgs(n, 0, 0, "rand_float() -> float"); return T.floatT; }
    if (f == "seed") {
        setB(BI::Seed);
        if (expectArgs(n, 0, 1, "seed(n: int)")) argT(0, T.intT);
        return T.voidT;
    }
    if (f == "chr") {
        setB(BI::Chr);
        if (expectArgs(n, 0, 1, "chr(code: int) -> str")) argT(0, T.intT);
        return T.strT;
    }
    if (f == "ord") {
        setB(BI::Ord);
        if (expectArgs(n, 0, 1, "ord(c: str) -> int")) argT(0, T.strT);
        return T.intT;
    }
    err(n->loc, "internal error: unknown builtin `" + f + "`");
    return T.errorT;
}

Type* Checker::builtinMethod(Node* n, Type* recv) {
    const std::string& m = n->str;
    auto argT = [&](size_t i, Type* want, const std::string& what = "") -> Type* {
        Type* t = expr(n->kids[i], want);
        if (want) { coerce(n->kids[i], want, what); return n->kids[i]->type; }
        return t;
    };
    auto sig = [&](const std::string& s) { return recv->str() + "." + s; };
    auto unknown = [&](const std::vector<std::string>& names) -> Type* {
        for (size_t i = 1; i < n->kids.size(); i++) expr(n->kids[i]);
        Diag& d = err(n->loc, "no method named `" + m + "` found for type `" + recv->str() + "`", "method not found");
        std::string s = suggest(m, names);
        if (!s.empty()) d.notes.push_back("help: did you mean `" + s + "`?");
        return T.errorT;
    };

    if (recv->kind == TK::Str) {
        static const std::vector<std::string> names = {"len", "is_empty", "contains", "starts_with", "ends_with", "find",
            "upper", "lower", "trim", "split", "split_whitespace", "lines", "replace", "repeat", "slice", "byte_at",
            "chars", "char_count", "to_int", "to_float"};
        struct M { BI b; std::vector<Type*> args; Type* ret; };
        Type* S = T.strT; Type* I = T.intT; Type* B = T.boolT;
        static std::map<std::string, M> table;
        if (table.empty()) {
            table["len"] = {BI::SLen, {}, I};
            table["is_empty"] = {BI::SIsEmpty, {}, B};
            table["contains"] = {BI::SContains, {S}, B};
            table["starts_with"] = {BI::SStartsWith, {S}, B};
            table["ends_with"] = {BI::SEndsWith, {S}, B};
            table["find"] = {BI::SFind, {S}, I};
            table["upper"] = {BI::SUpper, {}, S};
            table["lower"] = {BI::SLower, {}, S};
            table["trim"] = {BI::STrim, {}, S};
            table["split"] = {BI::SSplit, {S}, T.array(S)};
            table["split_whitespace"] = {BI::SSplitWs, {}, T.array(S)};
            table["lines"] = {BI::SLines, {}, T.array(S)};
            table["replace"] = {BI::SReplace, {S, S}, S};
            table["repeat"] = {BI::SRepeat, {I}, S};
            table["slice"] = {BI::SSlice, {I, I}, S};
            table["byte_at"] = {BI::SByteAt, {I}, I};
            table["chars"] = {BI::SChars, {}, T.array(S)};
            table["char_count"] = {BI::SCharCount, {}, I};
            table["to_int"] = {BI::SToInt, {}, T.optional(I)};
            table["to_float"] = {BI::SToFloat, {}, T.optional(T.floatT)};
        }
        auto it = table.find(m);
        if (it == table.end()) {
            if (m == "length" || m == "size" || m == "count") { Type* r = unknown(names); return r; }
            return unknown(names);
        }
        n->builtin = it->second.b;
        if (!expectArgs(n, 1, it->second.args.size(), sig(m) + "(...)")) return it->second.ret;
        for (size_t i = 0; i < it->second.args.size(); i++) argT(i + 1, it->second.args[i]);
        return it->second.ret;
    }

    if (recv->kind == TK::Array) {
        Type* E = recv->elem;
        static const std::vector<std::string> names = {"len", "is_empty", "push", "pop", "insert", "remove", "clear",
            "contains", "index_of", "reverse", "sort", "join", "slice", "copy", "last", "first"};
        auto mutating = [&]() { requireMutable(n->kids[0], n->kids[0]->loc, "call `" + m + "` on {}"); };
        if (m == "len") { n->builtin = BI::ALen; expectArgs(n, 1, 0, sig("len() -> int")); return T.intT; }
        if (m == "is_empty") { n->builtin = BI::AIsEmpty; expectArgs(n, 1, 0, sig("is_empty() -> bool")); return T.boolT; }
        if (m == "push" || m == "append") {
            n->builtin = BI::APush;
            mutating();
            if (expectArgs(n, 1, 1, sig("push(value: " + E->str() + ")"))) argT(1, E, "pushed value");
            return T.voidT;
        }
        if (m == "pop") { n->builtin = BI::APop; mutating(); expectArgs(n, 1, 0, sig("pop() -> " + E->str())); return E; }
        if (m == "insert") {
            n->builtin = BI::AInsert;
            mutating();
            if (expectArgs(n, 1, 2, sig("insert(index: int, value: " + E->str() + ")"))) { argT(1, T.intT); argT(2, E); }
            return T.voidT;
        }
        if (m == "remove") {
            n->builtin = BI::ARemove;
            mutating();
            if (expectArgs(n, 1, 1, sig("remove(index: int) -> " + E->str()))) argT(1, T.intT);
            return E;
        }
        if (m == "clear") { n->builtin = BI::AClear; mutating(); expectArgs(n, 1, 0, sig("clear()")); return T.voidT; }
        if (m == "contains" || m == "index_of") {
            n->builtin = m == "contains" ? BI::AContains : BI::AIndexOf;
            if (expectArgs(n, 1, 1, sig(m + "(value)"))) argT(1, E);
            if (E->kind != TK::Int && E->kind != TK::Float && E->kind != TK::Bool && E->kind != TK::Str && E->kind != TK::Struct)
                err(n->loc, "`" + m + "` is not supported for arrays of `" + E->str() + "`");
            return m == "contains" ? T.boolT : T.intT;
        }
        if (m == "reverse") { n->builtin = BI::AReverse; mutating(); expectArgs(n, 1, 0, sig("reverse()")); return T.voidT; }
        if (m == "sort") {
            n->builtin = BI::ASort;
            mutating();
            expectArgs(n, 1, 0, sig("sort()"));
            if (E->kind != TK::Int && E->kind != TK::Float && E->kind != TK::Str && E->kind != TK::Bool)
                err(n->loc, "`sort` requires an array of `int`, `float`, `str` or `bool`, found `" + recv->str() + "`");
            return T.voidT;
        }
        if (m == "join") {
            n->builtin = BI::AJoin;
            if (expectArgs(n, 1, 1, sig("join(sep: str) -> str"))) argT(1, T.strT);
            if (E->kind != TK::Str) {
                Diag& d = err(n->loc, "`join` requires an array of `str`, found `" + recv->str() + "`");
                d.notes.push_back("help: convert the elements first, e.g. with an f-string or `str(x)`");
            }
            return T.strT;
        }
        if (m == "slice") {
            n->builtin = BI::ASlice;
            if (expectArgs(n, 1, 2, sig("slice(start: int, end: int)"))) { argT(1, T.intT); argT(2, T.intT); }
            return recv;
        }
        if (m == "copy" || m == "clone") { n->builtin = BI::ACopy; expectArgs(n, 1, 0, sig("copy()")); return recv; }
        if (m == "last") { n->builtin = BI::ALast; expectArgs(n, 1, 0, sig("last() -> " + E->str())); return E; }
        if (m == "first") { n->builtin = BI::AFirst; expectArgs(n, 1, 0, sig("first() -> " + E->str())); return E; }
        return unknown(names);
    }

    if (recv->kind == TK::Map) {
        Type* K = recv->key;
        Type* V = recv->elem;
        static const std::vector<std::string> names = {"len", "is_empty", "contains", "get", "get_or", "remove", "keys", "values", "clear"};
        if (m == "len") { n->builtin = BI::MLen; expectArgs(n, 1, 0, sig("len() -> int")); return T.intT; }
        if (m == "is_empty") { n->builtin = BI::MIsEmpty; expectArgs(n, 1, 0, sig("is_empty() -> bool")); return T.boolT; }
        if (m == "contains" || m == "contains_key" || m == "has") {
            n->builtin = BI::MContains;
            if (expectArgs(n, 1, 1, sig("contains(key) -> bool"))) argT(1, K, "map key");
            return T.boolT;
        }
        if (m == "get") {
            n->builtin = BI::MGet;
            if (expectArgs(n, 1, 1, sig("get(key) -> ?" + V->str()))) argT(1, K, "map key");
            if (V->kind == TK::Optional) { err(n->loc, "`get` on a map with optional values is ambiguous; use `contains` and indexing"); return T.errorT; }
            return T.optional(V);
        }
        if (m == "get_or") {
            n->builtin = BI::MGetOr;
            if (expectArgs(n, 1, 2, sig("get_or(key, default) -> " + V->str()))) { argT(1, K, "map key"); argT(2, V); }
            return V;
        }
        if (m == "remove") {
            n->builtin = BI::MRemove;
            requireMutable(n->kids[0], n->kids[0]->loc, "call `remove` on {}");
            if (expectArgs(n, 1, 1, sig("remove(key) -> bool"))) argT(1, K, "map key");
            return T.boolT;
        }
        if (m == "keys") { n->builtin = BI::MKeys; expectArgs(n, 1, 0, sig("keys()")); return T.array(K); }
        if (m == "values") { n->builtin = BI::MValues; expectArgs(n, 1, 0, sig("values()")); return T.array(V); }
        if (m == "clear") {
            n->builtin = BI::MClear;
            requireMutable(n->kids[0], n->kids[0]->loc, "call `clear` on {}");
            expectArgs(n, 1, 0, sig("clear()"));
            return T.voidT;
        }
        return unknown(names);
    }

    if (recv->kind == TK::Optional) {
        Type* E = recv->elem;
        static const std::vector<std::string> names = {"unwrap", "is_some", "is_nil", "unwrap_or"};
        if (m == "unwrap" || m == "expect") {
            n->builtin = BI::OUnwrap;
            if (m == "expect") { if (expectArgs(n, 1, 1, sig("expect(message: str)"))) argT(1, T.strT); }
            else expectArgs(n, 1, 0, sig("unwrap() -> " + E->str()));
            return E;
        }
        if (m == "is_some") { n->builtin = BI::OIsSome; expectArgs(n, 1, 0, sig("is_some() -> bool")); return T.boolT; }
        if (m == "is_nil" || m == "is_none") { n->builtin = BI::OIsNil; expectArgs(n, 1, 0, sig("is_nil() -> bool")); return T.boolT; }
        if (m == "unwrap_or") {
            n->builtin = BI::OUnwrapOr;
            if (expectArgs(n, 1, 1, sig("unwrap_or(default) -> " + E->str()))) argT(1, E);
            return E;
        }
        // helpful: calling a method of the payload type
        for (size_t i = 1; i < n->kids.size(); i++) expr(n->kids[i]);
        Diag& d = err(n->loc, "no method named `" + m + "` found for optional type `" + recv->str() + "`");
        d.notes.push_back("help: the value may be nil; unwrap it first: `x.unwrap()." + m + "(...)` or `if let v = x { ... }`");
        return T.errorT;
    }

    for (size_t i = 1; i < n->kids.size(); i++) expr(n->kids[i]);
    err(n->loc, "type `" + recv->str() + "` has no methods", "no method `" + m + "`");
    return T.errorT;
}

}  // namespace arc
