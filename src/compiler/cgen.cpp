#include "cgen.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace arc {

// ---------------------------------------------------------------- naming --

// Injective: letters/digits are kept, `_` becomes `__`, every other byte `_xHH`.
// So in a mangled name `_` is always followed by `_` or `x`, and two different
// Arc names can never produce the same C identifier.
std::string CGen::mangle(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (std::isalnum(c)) r += (char)c;
        else if (c == '_') r += "__";
        else {
            char buf[8];
            std::snprintf(buf, sizeof buf, "_x%02X", c);
            r += buf;
        }
    }
    return r;
}

std::string CGen::cstrLit(const std::string& s) {
    std::string r = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\t': r += "\\t"; break;
        case '\r': r += "\\r"; break;
        case '?': r += "\\?"; break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\%03o", c);
                r += buf;
            } else {
                r += (char)c;
            }
        }
    }
    return r + "\"";
}

std::string CGen::structName(StructDecl* sd) { return "ArcS_" + mangle(sd->name); }

std::string CGen::fnName(FnDecl* fn) {
    // the owner's mangled length is spelled out so `A_b::c` and `A::b_c` differ
    if (fn->owner) {
        std::string owner = mangle(fn->owner->name);
        return "arc_m_" + std::to_string(owner.size()) + "_" + owner + "_" + mangle(fn->name);
    }
    return "arc_fn_" + mangle(fn->name);
}

std::string CGen::varName(Var* v) {
    if (!v->cname.empty()) return v->cname;
    if (v->isGlobal) v->cname = "arc_g_" + mangle(v->name);
    else if (v->isParam) v->cname = "a_" + mangle(v->name);
    else v->cname = "l_" + mangle(v->name) + "_" + std::to_string(++varCounter_);
    return v->cname;
}

std::string CGen::ctype(Type* t) {
    switch (t->kind) {
    case TK::Int: return "int64_t";
    case TK::Float: return "double";
    case TK::Bool: return "bool";
    case TK::Str: return "ArcStr *";
    case TK::Array: return "ArcArr *";
    case TK::Map: return "ArcMap *";
    case TK::Struct: case TK::Enum: return structName(t->sd) + " *";
    case TK::Void: return "void";
    case TK::Func: return "ArcClo *";
    case TK::Result:
        if (t->elem->kind == TK::Void) return "ArcErr *";
        if (t->elem->isHeap()) return "void *";
        resTypes_[t->id] = t;
        return "ArcRes_" + std::to_string(t->id);
    case TK::Optional:
        switch (t->elem->kind) {
        case TK::Int: return "ArcOptInt";
        case TK::Float: return "ArcOptFloat";
        case TK::Bool: return "ArcOptBool";
        default: return ctype(t->elem);
        }
    default: return "int64_t /*?*/";
    }
}

std::string CGen::td(Type* t) {
    switch (t->kind) {
    case TK::Func: return "&arc_td_fn";
    case TK::Int: return "&arc_td_int";
    case TK::Float: return "&arc_td_float";
    case TK::Bool: return "&arc_td_bool";
    case TK::Str: return "&arc_td_str";
    case TK::Optional:
        if (t->elem->kind == TK::Int) return "&arc_td_opt_int";
        if (t->elem->kind == TK::Float) return "&arc_td_opt_float";
        if (t->elem->kind == TK::Bool) return "&arc_td_opt_bool";
        if (t->elem->kind == TK::Str) return "&arc_td_opt_str";
        break;
    case TK::Array:
        if (t->elem->kind == TK::Int) return "&arc_td_arr_int";
        if (t->elem->kind == TK::Float) return "&arc_td_arr_float";
        if (t->elem->kind == TK::Bool) return "&arc_td_arr_bool";
        if (t->elem->kind == TK::Str) return "&arc_td_arr_str";
        break;
    default: break;
    }
    auto it = tdNames_.find(t->id);
    if (it != tdNames_.end()) return "&" + it->second;
    std::string name = "arc_td_" + std::to_string(t->id);
    tdNames_[t->id] = name;
    tdOrder_.push_back(t);
    // make sure nested descriptors exist too
    if (t->elem && t->elem->kind != TK::Void) td(t->elem);
    if (t->key) td(t->key);
    if (t->kind == TK::Struct)
        for (auto& f : t->sd->fields) td(f.type);
    return "&" + name;
}

std::string CGen::lit(const std::string& s) {
    auto it = strLits_.find(s);
    int idx;
    if (it != strLits_.end()) idx = it->second;
    else {
        idx = (int)strLitOrder_.size();
        strLits_[s] = idx;
        strLitOrder_.push_back(s);
    }
    return "(&arc_lit_" + std::to_string(idx) + ")";
}

std::string CGen::loc(const Loc& l) {
    return "ARC_LOC(" + std::to_string(std::max(l.file, 0)) + ", " + std::to_string(l.line) + ", " + std::to_string(l.col) + ")";
}

std::string CGen::srcText(const Loc& l) {
    if (l.file < 0 || l.file >= (int)prog_.files.size()) return "";
    const std::string& text = prog_.files[l.file]->text;
    if (l.off >= text.size()) return "";
    std::string s = text.substr(l.off, l.len);
    size_t nl = s.find('\n');
    if (nl != std::string::npos) s = s.substr(0, nl) + "...";
    return s;
}

std::string CGen::newTemp(Type* t) {
    std::string name = "t" + std::to_string(++tmpCounter_);
    temps_.push_back({name, t});
    return name;
}

std::string CGen::bind(Type* t, const std::string& cexpr) {
    std::string tmp = newTemp(t);
    lastBindStart_ = out_->tellp();
    line(tmp + " = " + cexpr + ";");
    lastBindEnd_ = out_->tellp();
    lastBindName_ = tmp;
    lastBindExpr_ = cexpr;
    return tmp;
}

// If `atom` is the temp produced by the most recent line, rewrite that line to
// store into `dest` instead and drop the temp. Returns false if not possible.
bool CGen::storeDirect(const std::string& atom, const std::string& dest) {
    if (atom.empty() || atom != lastBindName_ || out_->tellp() != lastBindEnd_) return false;
    std::string body = out_->str();
    body.resize((size_t)lastBindStart_);
    out_->str(body);
    out_->seekp(0, std::ios_base::end);
    line(dest + " = " + lastBindExpr_ + ";");
    for (auto& t : temps_)
        if (t.name == atom) t.type = T.voidT;  // no longer declared or rooted
    lastBindName_.clear();
    return true;
}

static bool isSimpleIdent(const std::string& s) {
    if (s.empty() || !(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (char c : s) if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

// Returns an addressable lvalue holding `atom`.
std::string CGen::lval(const std::string& atom, Type* t) {
    if (isSimpleIdent(atom) && atom != "true" && atom != "false" && atom != "NULL") return atom;
    return bind(t, atom);
}

std::string CGen::nilValue(Type* optT) {
    switch (optT->elem->kind) {
    case TK::Int: return "((ArcOptInt){false, 0})";
    case TK::Float: return "((ArcOptFloat){false, 0})";
    case TK::Bool: return "((ArcOptBool){false, false})";
    default: return "((" + ctype(optT) + ")NULL)";
    }
}

std::string CGen::someValue(Type* optT, const std::string& v) {
    switch (optT->elem->kind) {
    case TK::Int: return "((ArcOptInt){true, " + v + "})";
    case TK::Float: return "((ArcOptFloat){true, " + v + "})";
    case TK::Bool: return "((ArcOptBool){true, " + v + "})";
    default: return v;
    }
}

void CGen::line(const std::string& s) {
    for (int i = 0; i < indent_; i++) *out_ << "    ";
    *out_ << s << "\n";
}

// ------------------------------------------------------------- functions --

// How a local of type `t` is declared and (if it holds GC pointers) rooted.
static bool isValRes(Type* t) { return t->kind == TK::Result && !t->isHeap(); }
static std::string rootOf(Type* t, const std::string& name) { return isValRes(t) ? name + ".err" : name; }
static std::string clearOf(Type* t, const std::string& name) { return rootOf(t, name) + " = NULL;"; }

std::string CGen::prototype(FnDecl* fn) {
    if (fn->isLambda) {
        std::string s = "static " + ctype(fn->ret) + " arc_lam_" + std::to_string(fn->lambdaId) + "(ArcClo *env_";
        for (size_t i = 0; i < fn->params.size(); i++) s += ", " + ctype(fn->paramTypes[i]) + " " + varName(fn->params[i].var);
        return s + ")";
    }
    std::string s = "static " + ctype(fn->ret) + " " + fnName(fn) + "(";
    bool first = true;
    if (fn->hasSelf) {
        s += ctype(fn->owner->type) + " a_self";
        first = false;
    }
    for (size_t i = 0; i < fn->params.size(); i++) {
        if (!first) s += ", ";
        s += ctype(fn->paramTypes[i]) + " " + varName(fn->params[i].var);
        first = false;
    }
    if (first) s += "void";
    return s + ")";
}

std::string CGen::retStmt(const std::string& v) {
    std::string pop = hasFrame_ ? "arc_ftop = __fr.prev; " : "";
    if (v.empty()) return pop + "return;";
    return pop + "return " + v + ";";
}

std::string CGen::assembleBody(const std::string& sig, const std::string& body, Type* ret) {
    std::ostringstream o;
    o << sig << " {\n";
    std::vector<std::string> roots;
    // locals
    auto declare = [&](Type* t, const std::string& name) {
        std::string ct = ctype(t);
        if (t->isHeap()) {
            o << "    " << ct << " " << name << " = NULL;\n";
            roots.push_back(name);
        } else if (isValRes(t)) {
            o << "    " << ct << " " << name << " = {0};\n";
            roots.push_back(name + ".err");
        } else {
            o << "    " << ct << " " << name << ";\n";
        }
    };
    for (Var* v : fnVars_) declare(v->type, varName(v));
    for (auto& t : temps_) {
        if (t.type->kind == TK::Void) continue;
        declare(t.type, t.name);
    }
    if (fn_) {
        if (fn_->hasSelf) roots.push_back("a_self");
        for (size_t i = 0; i < fn_->params.size(); i++)
            if (fn_->paramTypes[i]->hasPtr()) roots.push_back(rootOf(fn_->paramTypes[i], varName(fn_->params[i].var)));
    }
    if (hasFrame_) {
        o << "    void **__slots[" << roots.size() << "] = {";
        for (size_t i = 0; i < roots.size(); i++) o << (i ? ", " : "") << "(void **)&" << roots[i];
        o << "};\n";
        o << "    ArcFrame __fr = {arc_ftop, " << roots.size() << ", __slots};\n";
        o << "    arc_ftop = &__fr;\n";
    }
    o << body;
    if (ret->kind == TK::Void && hasFrame_) o << "    arc_ftop = __fr.prev;\n";
    if (ret->kind == TK::Result && ret->elem->kind == TK::Void)  // `!void` falls off the end: success
        o << "    " << (hasFrame_ ? "arc_ftop = __fr.prev; " : "") << "return NULL;\n";
    o << "}\n\n";
    return o.str();
}

static bool needsFrame(const std::vector<Var*>& vars, const std::vector<std::pair<std::string, Type*>>& extra) {
    for (Var* v : vars) if (v->type->hasPtr()) return true;
    for (auto& e : extra) if (e.second->hasPtr()) return true;
    return false;
}

std::string CGen::genFunction(FnDecl* fn) {
    fn_ = fn;
    temps_.clear();
    lastBindName_.clear();
    fnVars_.clear();
    tmpCounter_ = 0;
    std::ostringstream body;
    out_ = &body;
    indent_ = 1;
    for (Var* v : fn->locals) if (!v->isParam) fnVars_.push_back(v);
    // decide on a frame before generating returns: generate twice if needed
    std::vector<std::pair<std::string, Type*>> params;
    if (fn->hasSelf) params.push_back({"a_self", fn->owner->type});
    for (size_t i = 0; i < fn->params.size(); i++) params.push_back({"", fn->paramTypes[i]});
    hasFrame_ = true;  // conservative first pass
    blockStmts(fn->body, false);
    bool anyHeapTemp = false;
    for (auto& t : temps_) if (t.type->hasPtr()) anyHeapTemp = true;
    bool need = anyHeapTemp || needsFrame(fnVars_, params);
    if (!need) {
        // regenerate without frame bookkeeping
        hasFrame_ = false;
        temps_.clear();
    lastBindName_.clear();
        tmpCounter_ = 0;
        body.str("");
        body.clear();
        blockStmts(fn->body, false);
    }
    out_ = nullptr;
    std::string r = assembleBody(prototype(fn), body.str(), fn->ret);
    fn_ = nullptr;
    return r;
}

std::string CGen::genGlobalInit() {
    fn_ = nullptr;
    temps_.clear();
    lastBindName_.clear();
    fnVars_.clear();
    tmpCounter_ = 0;
    hasFrame_ = true;
    std::ostringstream body;
    out_ = &body;
    indent_ = 1;
    for (Node* g : prog_.globalInitOrder) {
        std::string v = expr(g->kids[0]);
        line(varName(g->var) + " = " + v + ";");
    }
    out_ = nullptr;
    bool anyHeap = false;
    for (auto& t : temps_) if (t.type->hasPtr()) anyHeap = true;
    hasFrame_ = anyHeap;
    if (!anyHeap) {
        // no GC temps: plain body is fine (frame lines were never emitted inline)
    }
    return assembleBody("static void arc_init_globals(void)", body.str(), T.voidT);
}

// ------------------------------------------------------------ statements --

void CGen::blockStmts(Node* n, bool clearLocals) {
    for (Node* s : n->kids) stmt(s);
    if (!clearLocals || n->kids.empty()) return;
    NK last = n->kids.back()->kind;
    if (last == NK::Return || last == NK::Break || last == NK::Continue) return;
    // heap variables declared in this block are out of scope now: drop their roots
    for (Node* s : n->kids)
        if (s->kind == NK::Let && s->var && s->var->type->hasPtr()) line(clearOf(s->var->type, varName(s->var)));
}

// Generates one statement, then clears the GC roots it no longer needs: every
// heap temporary it created and any loop / if-let variable it introduced.
// Without this, a dead object stays reachable until the function returns.
void CGen::stmt(Node* n) {
    size_t mark = temps_.size();
    stmtInner(n);
    if (n->kind == NK::Return || n->kind == NK::Break || n->kind == NK::Continue) return;
    for (size_t i = mark; i < temps_.size(); i++) {
        Temp& t = temps_[i];
        if (t.cleared || t.type->kind == TK::Void || !t.type->hasPtr()) continue;
        t.cleared = true;
        line(clearOf(t.type, t.name));
    }
    if (n->kind != NK::Let) {
        if (n->var && n->var->type->hasPtr()) line(clearOf(n->var->type, varName(n->var)));
        if (n->var2 && n->var2->type->hasPtr()) line(clearOf(n->var2->type, varName(n->var2)));
    }
}

void CGen::stmtInner(Node* n) {
    switch (n->kind) {
    case NK::Let: {
        std::string v = expr(n->kids[0]);
        if (!storeDirect(v, varName(n->var))) line(varName(n->var) + " = " + v + ";");
        return;
    }
    case NK::Assign: assign(n); return;
    case NK::ExprStmt: expr(n->kids[0]); return;
    case NK::Match: genMatch(n, ""); return;
    case NK::If: {
        std::string c = expr(n->kids[0]);
        open("if (" + c + ")");
        blockStmts(n->kids[1]);
        if (n->kids[2]) {
            indent_--;
            line("} else {");
            indent_++;
            if (n->kids[2]->kind == NK::Block) blockStmts(n->kids[2]);
            else stmt(n->kids[2]);
        }
        close();
        return;
    }
    case NK::IfLet: {
        Type* ot = n->kids[0]->type;
        std::string o = expr(n->kids[0]);
        if (ot->kind == TK::Result) {
            o = lval(o, ot);
            open("if (!" + resIsErr(ot, o) + ")");
            line(varName(n->var) + " = " + resVal(ot, o) + ";");
        } else {
            std::string cond = ot->elem->isHeap() ? "(" + o + ") != NULL" : "(" + o + ").has";
            open("if (" + cond + ")");
            line(varName(n->var) + " = " + (ot->elem->isHeap() ? o : "(" + o + ").val") + ";");
        }
        blockStmts(n->kids[1]);
        if (n->kids[2]) {
            indent_--;
            line("} else {");
            indent_++;
            if (n->kids[2]->kind == NK::Block) blockStmts(n->kids[2]);
            else stmt(n->kids[2]);
        }
        close();
        return;
    }
    case NK::While: {
        Node* c = n->kids[0];
        if (c->kind == NK::BoolLit && c->flag) {
            open("for (;;)");
        } else {
            open("for (;;)");
            std::string cv = expr(c);
            line("if (!(" + cv + ")) break;");
        }
        blockStmts(n->kids[1]);
        close();
        return;
    }
    case NK::Loop:
        open("for (;;)");
        blockStmts(n->kids[0]);
        close();
        return;
    case NK::ForRange: {
        std::string a = expr(n->kids[0]);
        std::string b = expr(n->kids[1]);
        std::string s = newTemp(T.intT), e = newTemp(T.intT);
        line(s + " = " + a + ";");
        line(e + " = " + b + ";");
        std::string k = "k" + std::to_string(++tmpCounter_);
        if (n->flag) {
            // inclusive: test for the last value before incrementing so that an
            // upper bound of INT64_MAX terminates (and `continue` still works)
            std::string done = "d" + std::to_string(++tmpCounter_);
            open("for (int64_t " + k + " = " + s + ", " + done + " = " + s + " > " + e + "; !" + done + "; " + done + " = " + k +
                 " == " + e + ", " + k + "++)");
        } else {
            open("for (int64_t " + k + " = " + s + "; " + k + " < " + e + "; " + k + "++)");
        }
        line(varName(n->var) + " = " + k + ";");
        blockStmts(n->kids[2]);
        close();
        return;
    }
    case NK::ForEach: {
        Type* it = n->kids[0]->type;
        std::string coll = expr(n->kids[0]);
        std::string k = "k" + std::to_string(++tmpCounter_);
        bool two = n->var2 != nullptr;
        if (it->kind == TK::Map) {
            std::string m = bind(it, coll);
            open("for (int64_t " + k + " = arc_map_next(" + m + ", 0); " + k + " >= 0; " + k + " = arc_map_next(" + m + ", " + k + " + 1))");
            line(varName(n->var) + " = *(" + ctype(it->key) + " *)arc_map_ekey(" + m + ", " + k + ");");
            if (two) line(varName(n->var2) + " = *(" + ctype(it->elem) + " *)arc_map_eval(" + m + ", " + k + ");");
            blockStmts(n->kids[1]);
            close();
            return;
        }
        std::string a;
        Type* et;
        if (it->kind == TK::Str) {
            a = bind(T.array(T.strT), "arc_str_chars(" + coll + ")");
            et = T.strT;
        } else {
            a = bind(it, coll);
            et = it->elem;
        }
        open("for (int64_t " + k + " = 0; " + k + " < " + a + "->len; " + k + "++)");
        if (two) {
            line(varName(n->var) + " = " + k + ";");
            line(varName(n->var2) + " = ((" + ctype(et) + " *)" + a + "->data)[" + k + "];");
        } else {
            line(varName(n->var) + " = ((" + ctype(et) + " *)" + a + "->data)[" + k + "];");
        }
        blockStmts(n->kids[1]);
        close();
        return;
    }
    case NK::Break: line("break;"); return;
    case NK::Continue: line("continue;"); return;
    case NK::Return: {
        bool resVoid = fn_ && fn_->ret->kind == TK::Result && fn_->ret->elem->kind == TK::Void;
        if (n->kids.empty()) { line(retStmt(resVoid ? "NULL" : "")); return; }
        std::string v = expr(n->kids[0]);
        if (n->isConst || (fn_ && fn_->ret->kind == TK::Void)) { line(retStmt("")); return; }
        line(retStmt(v));
        return;
    }
    case NK::Block:
        open("");
        blockStmts(n);
        close();
        return;
    default:
        line("/* unsupported statement */");
    }
}

std::string CGen::assign(Node* n) {
    Node* target = n->kids[0];
    Type* tt = target->type;
    std::string op = n->str == "=" ? "" : n->str.substr(0, n->str.size() - 1);
    std::string v = expr(n->kids[1]);
    auto combine = [&](const std::string& cur) -> std::string {
        if (op.empty()) return v;
        return arith(op, tt, cur, v, n->loc);
    };
    switch (target->kind) {
    case NK::Ident: {
        std::string name = varName(target->var);
        if (op.empty() && storeDirect(v, name)) break;
        line(name + " = " + combine(name) + ";");
        break;
    }
    case NK::Field: {
        std::string o = expr(target->kids[0]);
        std::string place = o + "->f_" + mangle(target->str);
        line(place + " = " + combine(place) + ";");
        break;
    }
    case NK::Index: {
        Type* ct = target->kids[0]->type;
        std::string c = expr(target->kids[0]);
        if (ct->kind == TK::Map) {
            std::string k = lval(expr(target->kids[1]), ct->key);
            std::string ptr = newTemp(T.intT);  // placeholder name; declare pointer separately
            temps_.back().type = T.voidT;       // not declared via temps
            std::string vt = ctype(ct->elem);
            if (op.empty()) {
                line("*(" + vt + " *)arc_map_slot(" + c + ", &" + k + ") = " + v + ";");
            } else {
                line("{ " + vt + " *" + ptr + " = (" + vt + " *)arc_map_must(" + c + ", &" + k + ", " + loc(target->loc) + ");");
                line("  *" + ptr + " = " + combine("*" + ptr) + "; }");
            }
        } else {
            std::string i = expr(target->kids[1]);
            std::string idx = newTemp(T.intT);
            line(idx + " = arc_idx(" + c + ", " + i + ", " + loc(target->loc) + ");");
            std::string place = "((" + ctype(ct->elem) + " *)" + c + "->data)[" + idx + "]";
            line(place + " = " + combine(place) + ";");
        }
        break;
    }
    default:
        line("/* invalid assignment */");
    }
    return "";
}

// ----------------------------------------------------------- expressions --

static std::string floatLit(double v) {
    if (std::isinf(v)) return v > 0 ? "(1.0/0.0)" : "(-1.0/0.0)";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    std::string s = buf;
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('n') == std::string::npos) s += ".0";
    return "(" + s + ")";
}

std::string CGen::expr(Node* n) {
    switch (n->kind) {
    case NK::IntLit:
        if (n->ival < 0) return "((int64_t)(" + std::to_string(n->ival) + "))";
        return "((int64_t)" + std::to_string(n->ival) + ")";
    case NK::FloatLit: return floatLit(n->fval);
    case NK::BoolLit: return n->flag ? "true" : "false";
    case NK::StrLit: return lit(n->str);
    case NK::NilLit: return nilValue(n->type);
    case NK::Wrap: {
        std::string v = expr(n->kids[0]);
        if (n->type->kind == TK::Result) {
            if (n->type->isHeap()) return bind(n->type, "(void *)" + v);
            return bind(n->type, "((" + ctype(n->type) + "){NULL, " + v + "})");
        }
        if (n->type->elem->isHeap()) return v;
        return bind(n->type, someValue(n->type, v));
    }
    case NK::Match: {
        std::string r = newTemp(n->type);
        genMatch(n, r);
        return r;
    }
    case NK::Try: return genTry(n);
    case NK::Path:
        return "((" + structName(n->type->sd) + " *)&arc_ev_" + mangle(n->type->sd->name) + "_" + std::to_string(n->ival) + ")";
    case NK::Lambda: {
        FnDecl* L = n->fn;
        std::string id = std::to_string(L->lambdaId);
        std::string env = "ArcL_" + id;
        std::string t = bind(n->type, "(ArcClo *)arc_alloc(&arc_td_lam_" + id + ", sizeof(" + env + "))");
        line(t + "->fn = (void *)arc_lam_" + id + ";");
        for (size_t i = 0; i < L->captures.size(); i++)
            line("((" + env + " *)" + t + ")->c_" + std::to_string(i) + " = " + varName(L->captures[i].first) + ";");
        return t;
    }
    case NK::FStr: {
        std::vector<std::string> atoms;
        for (Node* k : n->kids) atoms.push_back(expr(k));
        std::string b = "fb" + std::to_string(++tmpCounter_);
        line("{");
        indent_++;
        line("ArcBuf " + b + ";");
        line("arc_buf_init(&" + b + ");");
        for (size_t i = 0; i < n->names.size(); i++) {
            if (!n->names[i].empty())
                line("arc_buf_put(&" + b + ", " + cstrLit(n->names[i]) + ", " + std::to_string(n->names[i].size()) + ");");
            if (i < atoms.size()) fmtInto(b, atoms[i], n->kids[i]->type, n->specs[i], false);
        }
        std::string t = newTemp(T.strT);
        line(t + " = arc_str_from_buf(&" + b + ");");
        indent_--;
        line("}");
        return t;
    }
    case NK::Ident: {
        if (n->fn) {  // a named function used as a value
            fnValues_.insert(n->fn);
            return "((ArcClo *)&arc_fv_" + fnName(n->fn) + ")";
        }
        Var* v = n->var;
        if (v->isGlobal) return bind(v->type, varName(v));
        return varName(v);
    }
    case NK::Unary: {
        std::string a = expr(n->kids[0]);
        if (n->str == "-") return bind(n->type, "(-" + a + ")");
        if (n->str == "!") return bind(n->type, "(!" + a + ")");
        return bind(n->type, "(~" + a + ")");
    }
    case NK::Binary: return binaryExpr(n);
    case NK::Call: case NK::MethodCall: case NK::PathCall: return callExpr(n);
    case NK::Field: {
        std::string o = expr(n->kids[0]);
        return bind(n->type, o + "->f_" + mangle(n->str));
    }
    case NK::Index: {
        Type* ct = n->kids[0]->type;
        std::string c = expr(n->kids[0]);
        if (ct->kind == TK::Map) {
            std::string k = lval(expr(n->kids[1]), ct->key);
            return bind(n->type, "*(" + ctype(ct->elem) + " *)arc_map_must(" + c + ", &" + k + ", " + loc(n->loc) + ")");
        }
        std::string i = expr(n->kids[1]);
        return bind(n->type, "((" + ctype(ct->elem) + " *)" + c + "->data)[arc_idx(" + c + ", " + i + ", " + loc(n->loc) + ")]");
    }
    case NK::Slice: {
        Type* ct = n->kids[0]->type;
        std::string c = expr(n->kids[0]);
        std::string a = n->kids[1] ? expr(n->kids[1]) : "((int64_t)0)";
        std::string b = n->kids[2] ? expr(n->kids[2]) : c + "->len";
        if (n->flag && n->kids[2]) b = "(" + b + " == INT64_MAX ? INT64_MAX : " + b + " + 1)";
        if (ct->kind == TK::Str) return bind(n->type, "arc_str_slice(" + c + ", " + a + ", " + b + ", " + loc(n->loc) + ")");
        return bind(n->type, "arc_arr_slice(" + c + ", " + a + ", " + b + ", " + loc(n->loc) + ")");
    }
    case NK::ArrayLit: {
        std::vector<std::string> atoms;
        for (Node* k : n->kids) atoms.push_back(expr(k));
        std::string a = bind(n->type, "arc_arr_new(" + td(n->type) + ", " + std::to_string(atoms.size()) + ", " + std::to_string(atoms.size()) + ")");
        std::string et = ctype(n->type->elem);
        for (size_t i = 0; i < atoms.size(); i++)
            line("((" + et + " *)" + a + "->data)[" + std::to_string(i) + "] = " + atoms[i] + ";");
        return a;
    }
    case NK::ArrayRepeat: {
        std::string v = expr(n->kids[0]);
        std::string c = expr(n->kids[1]);
        std::string cnt = bind(T.intT, "arc_check_len(" + c + ", " + loc(n->kids[1]->loc) + ")");
        std::string a = bind(n->type, "arc_arr_new(" + td(n->type) + ", " + cnt + ", " + cnt + ")");
        std::string et = ctype(n->type->elem);
        bool zero = (n->kids[0]->kind == NK::IntLit && n->kids[0]->ival == 0) ||
                    (n->kids[0]->kind == NK::BoolLit && !n->kids[0]->flag) ||
                    (n->kids[0]->kind == NK::FloatLit && n->kids[0]->fval == 0 && !std::signbit(n->kids[0]->fval));
        if (!zero) {
            std::string k = "k" + std::to_string(++tmpCounter_);
            line("for (int64_t " + k + " = 0; " + k + " < " + cnt + "; " + k + "++) ((" + et + " *)" + a + "->data)[" + k + "] = " + v + ";");
        }
        return a;
    }
    case NK::MapLit: {
        std::vector<std::string> atoms;
        for (size_t i = 0; i < n->kids.size(); i++) {
            std::string a = expr(n->kids[i]);
            if (i % 2 == 0) a = lval(a, n->type->key);
            atoms.push_back(a);
        }
        std::string m = bind(n->type, "arc_map_new(" + td(n->type) + ")");
        std::string vt = ctype(n->type->elem);
        for (size_t i = 0; i < atoms.size(); i += 2)
            line("*(" + vt + " *)arc_map_slot(" + m + ", &" + atoms[i] + ") = " + atoms[i + 1] + ";");
        return m;
    }
    case NK::StructLit: {
        StructDecl* sd = n->type->sd;
        std::vector<std::string> atoms;
        for (Node* k : n->kids) atoms.push_back(expr(k));
        std::string s = bind(n->type, "(" + structName(sd) + " *)arc_new_struct(" + td(n->type) + ")");
        for (size_t i = 0; i < atoms.size(); i++) line(s + "->f_" + mangle(n->names[i]) + " = " + atoms[i] + ";");
        return s;
    }
    case NK::Cast: {
        Type* from = n->kids[0]->type;
        Type* to = n->type;
        std::string a = expr(n->kids[0]);
        if (from == to) return a;
        if (to->kind == TK::Float) return bind(to, "((double)" + a + ")");
        if (to->kind == TK::Int && from->kind == TK::Float) return bind(to, "arc_f2i(" + a + ")");
        return bind(to, "((int64_t)" + a + ")");
    }
    default:
        return "0 /* unsupported expression */";
    }
}

// ---- results: how `!T` values are represented in C
//   !void          ArcErr *            (NULL = success)
//   !T, T heap     void *              (a T pointer, or an ArcErr pointer)
//   !T, T value    ArcRes_N {err, val}
std::string CGen::resIsErr(Type* rt, const std::string& a) {
    if (rt->elem->kind == TK::Void) return "(" + a + " != NULL)";
    if (rt->isHeap()) return "arc_is_err(" + a + ")";
    return "(" + a + ".err != NULL)";
}
std::string CGen::resErr(Type* rt, const std::string& a) {
    if (rt->elem->kind == TK::Void) return a;
    if (rt->isHeap()) return "((ArcErr *)" + a + ")";
    return a + ".err";
}
std::string CGen::resVal(Type* rt, const std::string& a) {
    if (rt->isHeap()) return "((" + ctype(rt->elem) + ")" + a + ")";
    return a + ".val";
}
std::string CGen::resFromErr(Type* rt, const std::string& e) {
    if (rt->elem->kind == TK::Void) return e;
    if (rt->isHeap()) return "(void *)" + e;
    return "((" + ctype(rt) + "){" + e + "})";
}

std::string CGen::callClosure(Type* ft, const std::string& f, const std::vector<std::string>& args) {
    std::string sig = ctype(ft->elem) + " (*)(ArcClo *";
    for (Type* p : ft->params) sig += ", " + ctype(p);
    sig += ")";
    std::string c = "((" + sig + ")" + f + "->fn)(" + f;
    for (auto& a : args) c += ", " + a;
    return c + ")";
}

// `x?`: on an error (or nil) return it from the current function
std::string CGen::genTry(Node* n) {
    Type* ot = n->kids[0]->type;
    Type* fr = fn_->ret;
    std::string a = bind(ot, expr(n->kids[0]));
    if (ot->kind == TK::Result) {
        open("if (" + resIsErr(ot, a) + ")");
        line(retStmt(resFromErr(fr, resErr(ot, a))));
        close();
        if (ot->elem->kind == TK::Void) return "";
        return bind(ot->elem, resVal(ot, a));
    }
    std::string isNil = ot->elem->isHeap() ? "(" + a + " == NULL)" : "!" + a + ".has";
    open("if (" + isNil + ")");
    line(retStmt(nilValue(fr)));
    close();
    return bind(ot->elem, ot->elem->isHeap() ? a : a + ".val");
}

// Condition under which pattern `p` matches value `v` of type `t` (pure C).
std::string CGen::patCond(Node* p, Type* t, const std::string& v) {
    const std::string& k = p->str;
    if (k == "_" || k == "bind") return "1";
    if (k == "lit") {
        std::string lv = expr(p->kids[0]);
        return t->kind == TK::Str ? "arc_str_eq(" + v + ", " + lv + ")" : "(" + v + " == " + lv + ")";
    }
    if (k == "ok" || k == "err") {
        std::string c = k == "ok" ? "!" + resIsErr(t, v) : resIsErr(t, v);
        Node* sub = p->kids.empty() ? nullptr : p->kids[0];
        if (!sub || irrefutablePat(sub) || (k == "ok" && t->elem->kind == TK::Void)) return c;
        std::string inner = k == "ok" ? patCond(sub, t->elem, resVal(t, v)) : patCond(sub, T.strT, resErr(t, v) + "->msg");
        return "(" + c + " && " + inner + ")";
    }
    StructDecl* sd = t->sd;
    Variant& var = sd->variants[p->ival];
    std::string c = "(" + v + "->tag == " + std::to_string(p->ival);
    for (size_t i = 0; i < p->kids.size(); i++) {
        if (irrefutablePat(p->kids[i])) continue;
        c += " && " + patCond(p->kids[i], var.types[i], v + "->f_" + mangle(sd->fields[var.firstField + i].name));
    }
    return c + ")";
}

// Assigns the bindings of a matched pattern.
void CGen::patBind(Node* p, Type* t, const std::string& v) {
    const std::string& k = p->str;
    if (k == "bind") { line(varName(p->vars[0]) + " = " + v + ";"); return; }
    if ((k == "ok" || k == "err") && !p->kids.empty()) {
        if (k == "ok") { if (t->elem->kind != TK::Void) patBind(p->kids[0], t->elem, resVal(t, v)); }
        else patBind(p->kids[0], T.strT, resErr(t, v) + "->msg");
        return;
    }
    if (k == "variant") {
        StructDecl* sd = t->sd;
        Variant& var = sd->variants[p->ival];
        for (size_t i = 0; i < p->kids.size(); i++)
            patBind(p->kids[i], var.types[i], v + "->f_" + mangle(sd->fields[var.firstField + i].name));
    }
}

bool CGen::irrefutablePat(Node* p) { return p->str == "_" || p->str == "bind"; }

// match: test arms in order; `taken` records that one has been taken
void CGen::genMatch(Node* m, const std::string& result) {
    Type* st = m->kids[0]->type;
    std::string sv = bind(st, expr(m->kids[0]));
    std::string taken = bind(T.boolT, "false");
    for (Node* arm : m->pats) {
        std::string cond;
        for (Node* p : arm->pats) cond += (cond.empty() ? "" : " || ") + patCond(p, st, sv);
        open("if (!" + taken + " && (" + cond + "))");
        patBind(arm->pats[0], st, sv);
        Node* guard = arm->kids[1];
        if (guard) {
            std::string g = expr(guard);
            open("if (" + g + ")");
        }
        line(taken + " = true;");
        Node* body = arm->kids[0];
        if (!result.empty() && body->kind != NK::Block) {
            std::string v = expr(body);
            line(result + " = " + v + ";");
        } else if (body->kind == NK::Block) {
            blockStmts(body);
        } else {
            stmt(body);
        }
        if (guard) close();
        close();
    }
}

std::string CGen::arith(const std::string& op, Type* t, const std::string& a, const std::string& b, const Loc& l) {
    if (t->kind == TK::Str) return "arc_str_concat(" + a + ", " + b + ")";
    if (t->kind == TK::Float) {
        if (op == "%") return "fmod(" + a + ", " + b + ")";
        return "(" + a + " " + op + " " + b + ")";
    }
    if (op == "/") return "arc_div(" + a + ", " + b + ", " + loc(l) + ")";
    if (op == "%") return "arc_mod(" + a + ", " + b + ", " + loc(l) + ")";
    if (op == "<<") return "arc_shl(" + a + ", " + b + ", " + loc(l) + ")";
    if (op == ">>") return "arc_shr(" + a + ", " + b + ", " + loc(l) + ")";
    return "(" + a + " " + op + " " + b + ")";
}

std::string CGen::eqExpr(Type* t, const std::string& a, const std::string& b) {
    switch (t->kind) {
    case TK::Int: case TK::Float: case TK::Bool: return "(" + a + " == " + b + ")";
    case TK::Str: return "arc_str_eq(" + a + ", " + b + ")";
    default: {
        std::string la = lval(a, t), lb = lval(b, t);
        return "arc_deep_eq(&" + la + ", &" + lb + ", " + td(t) + ", 0)";
    }
    }
}

std::string CGen::binaryExpr(Node* n) {
    const std::string& op = n->str;
    if (op == "&&" || op == "||") {
        std::string a = expr(n->kids[0]);
        std::string t = bind(T.boolT, a);
        open(op == "&&" ? "if (" + t + ")" : "if (!" + t + ")");
        std::string b = expr(n->kids[1]);
        line(t + " = " + b + ";");
        close();
        return t;
    }
    if (op == "??" && n->kids[0]->type->kind == TK::Result) {
        Type* ot = n->kids[0]->type;
        std::string a = lval(expr(n->kids[0]), ot);
        std::string t = newTemp(n->type);
        open("if (!" + resIsErr(ot, a) + ")");
        line(t + " = " + resVal(ot, a) + ";");
        indent_--;
        line("} else {");
        indent_++;
        std::string b = expr(n->kids[1]);
        line(t + " = " + b + ";");
        close();
        return t;
    }
    if (op == "??") {
        Type* ot = n->kids[0]->type;
        std::string a = lval(expr(n->kids[0]), ot);
        std::string t = newTemp(n->type);
        bool resultOpt = n->type->kind == TK::Optional;
        std::string has = ot->elem->isHeap() ? a + " != NULL" : a + ".has";
        std::string val = resultOpt ? a : (ot->elem->isHeap() ? a : a + ".val");
        open("if (" + has + ")");
        line(t + " = " + val + ";");
        indent_--;
        line("} else {");
        indent_++;
        std::string b = expr(n->kids[1]);
        line(t + " = " + b + ";");
        close();
        return t;
    }
    Node* L = n->kids[0];
    Node* R = n->kids[1];
    // comparisons against nil
    if ((op == "==" || op == "!=") && (L->kind == NK::NilLit || R->kind == NK::NilLit)) {
        Node* other = L->kind == NK::NilLit ? R : L;
        Type* ot = other->type;
        std::string a = expr(other);
        if (ot->kind != TK::Optional) return op == "==" ? "false" : "true";
        std::string isNil = ot->elem->isHeap() ? "(" + a + " == NULL)" : "(!(" + a + ").has)";
        return bind(T.boolT, op == "==" ? isNil : "(!" + isNil + ")");
    }
    std::string a = expr(L);
    std::string b = expr(R);
    Type* t = L->type;
    if (op == "==" || op == "!=") {
        std::string e = eqExpr(t, a, b);
        return bind(T.boolT, op == "==" ? e : "(!" + e + ")");
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (t->kind == TK::Str) return bind(T.boolT, "(arc_str_cmp(" + a + ", " + b + ") " + op + " 0)");
        return bind(T.boolT, "(" + a + " " + op + " " + b + ")");
    }
    return bind(n->type, arith(op, t, a, b, n->loc));
}

void CGen::fmtInto(const std::string& buf, const std::string& atom, Type* t, const std::string& spec, bool quoted) {
    char fill = ' ', align = 0;
    int64_t width = -1;
    int prec = -1;
    size_t i = 0;
    auto isAlign = [](char c) { return c == '<' || c == '>' || c == '^'; };
    if (spec.size() >= 2 && isAlign(spec[1])) { fill = spec[0]; align = spec[1]; i = 2; }
    else if (!spec.empty() && isAlign(spec[0])) { align = spec[0]; i = 1; }
    if (i < spec.size() && spec[i] == '0' && align == 0) { fill = '0'; align = '>'; i++; }
    std::string w;
    while (i < spec.size() && std::isdigit((unsigned char)spec[i])) w += spec[i++];
    if (!w.empty()) width = std::stoll(w);
    if (i < spec.size() && spec[i] == '.') {
        i++;
        std::string p;
        while (i < spec.size() && std::isdigit((unsigned char)spec[i])) p += spec[i++];
        prec = std::stoi(p);
    }
    std::string st;
    if (width >= 0) {
        st = "fs" + std::to_string(++tmpCounter_);
        line("size_t " + st + " = " + buf + ".len;");
    }
    std::string B = "&" + buf;
    if (prec >= 0 && t->kind == TK::Float) line("arc_buf_float_prec(" + B + ", " + atom + ", " + std::to_string(prec) + ");");
    else switch (t->kind) {
    case TK::Int: line("arc_buf_int(" + B + ", " + atom + ");"); break;
    case TK::Float: line("arc_buf_float(" + B + ", " + atom + ");"); break;
    case TK::Bool: line("arc_buf_bool(" + B + ", " + atom + ");"); break;
    case TK::Str: line(std::string(quoted ? "arc_buf_quoted(" : "arc_buf_str(") + B + ", " + atom + ");"); break;
    default: {
        std::string lv = lval(atom, t);
        line("arc_fmt_value(" + B + ", &" + lv + ", " + td(t) + ", " + (quoted ? "1" : "0") + ", 0);");
    }
    }
    if (width >= 0) {
        if (!align) align = t->isNumeric() ? '>' : '<';
        std::string f = "(char)" + std::to_string((int)(unsigned char)fill);  // any byte, safely
        line("arc_buf_align(" + B + ", " + st + ", " + std::to_string(width) + ", '" + align + "', " + f + ");");
    }
}

std::string CGen::callExpr(Node* n) {
    if (n->kind == NK::PathCall && n->flag) {  // Enum::Variant(payload...)
        StructDecl* sd = n->type->sd;
        Variant& v = sd->variants[n->ival];
        std::vector<std::string> atoms;
        for (Node* k : n->kids) atoms.push_back(expr(k));
        std::string t = bind(n->type, "(" + structName(sd) + " *)arc_new_struct(" + td(n->type) + ")");
        line(t + "->tag = " + std::to_string(n->ival) + ";");
        for (size_t i = 0; i < atoms.size(); i++)
            line(t + "->f_" + mangle(sd->fields[v.firstField + i].name) + " = " + atoms[i] + ";");
        return t;
    }
    if (n->builtin == BI::CallValue) {
        Type* ft;
        std::string f;
        size_t first;
        if (n->kind == NK::Call && n->isConst) {
            ft = n->kids[0]->type;
            f = bind(ft, expr(n->kids[0]));
            first = 1;
        } else if (n->kind == NK::Call) {
            ft = n->var->type;
            f = n->var->isGlobal ? bind(ft, varName(n->var)) : varName(n->var);
            first = 0;
        } else {
            StructDecl* sd = n->kids[0]->type->sd;
            ft = sd->fields[n->ival].type;
            std::string recv = expr(n->kids[0]);
            f = bind(ft, recv + "->f_" + mangle(sd->fields[n->ival].name));
            first = 1;
        }
        std::vector<std::string> args;
        for (size_t i = first; i < n->kids.size(); i++) args.push_back(expr(n->kids[i]));
        std::string c = callClosure(ft, f, args);
        if (ft->elem->kind == TK::Void) { line(c + ";"); return ""; }
        return bind(ft->elem, c);
    }
    if (n->builtin != BI::None) return n->kind == NK::Call ? builtinCall(n) : builtinMethod(n);
    FnDecl* fn = n->fn;
    std::vector<std::string> atoms;
    for (Node* k : n->kids) atoms.push_back(expr(k));
    std::string c = fnName(fn) + "(";
    for (size_t i = 0; i < atoms.size(); i++) c += (i ? ", " : "") + atoms[i];
    c += ")";
    if (fn->ret->kind == TK::Void) {
        line(c + ";");
        return "";
    }
    return bind(fn->ret, c);
}

std::string CGen::builtinCall(Node* n) {
    std::vector<std::string> a;
    for (Node* k : n->kids) a.push_back(expr(k));
    auto ty = [&](size_t i) { return n->kids[i]->type; };
    std::string L = loc(n->loc);
    switch (n->builtin) {
    case BI::Print: case BI::Println: case BI::Eprint: case BI::Eprintln: {
        bool nl = n->builtin == BI::Println || n->builtin == BI::Eprintln;
        bool err = n->builtin == BI::Eprint || n->builtin == BI::Eprintln;
        if (a.size() == 1 && ty(0)->kind == TK::Str && !err) {
            line(std::string("fwrite(") + a[0] + "->data, 1, (size_t)" + a[0] + "->len, stdout);");
            if (nl) line("putchar('\\n');");
            return "";
        }
        for (size_t i = 0; i < a.size(); i++) {
            if (i) line("arc_buf_putc(&arc_pb, ' ');");
            fmtInto("arc_pb", a[i], ty(i), "", false);
        }
        if (nl) line("arc_buf_putc(&arc_pb, '\\n');");
        line(std::string("arc_out(") + (err ? "stderr" : "stdout") + ");");
        return "";
    }
    case BI::Str: {
        Type* t = ty(0);
        if (t->kind == TK::Str) return a[0];
        if (t->kind == TK::Int) return bind(T.strT, "arc_str_of_int(" + a[0] + ")");
        if (t->kind == TK::Float) return bind(T.strT, "arc_str_of_float(" + a[0] + ")");
        if (t->kind == TK::Bool) return bind(T.strT, "arc_str_of_bool(" + a[0] + ")");
        std::string lv = lval(a[0], t);
        return bind(T.strT, "arc_str_of_value(&" + lv + ", " + td(t) + ")");
    }
    case BI::Int: {
        Type* t = ty(0);
        if (t->kind == TK::Int) return a[0];
        if (t->kind == TK::Float) return bind(T.intT, "arc_f2i(" + a[0] + ")");
        return bind(T.intT, "((int64_t)" + a[0] + ")");
    }
    case BI::Float:
        if (ty(0)->kind == TK::Float) return a[0];
        return bind(T.floatT, "((double)" + a[0] + ")");
    case BI::Len: return bind(T.intT, a[0] + "->len");
    case BI::ParseInt: return bind(n->type, "arc_parse_int(" + a[0] + ")");
    case BI::ParseFloat: return bind(n->type, "arc_parse_float(" + a[0] + ")");
    case BI::Abs: return bind(n->type, n->type->kind == TK::Int ? "arc_iabs(" + a[0] + ")" : "fabs(" + a[0] + ")");
    case BI::Min: case BI::Max: {
        bool mn = n->builtin == BI::Min;
        if (n->type->kind == TK::Int) return bind(n->type, std::string(mn ? "arc_imin(" : "arc_imax(") + a[0] + ", " + a[1] + ")");
        if (n->type->kind == TK::Float) return bind(n->type, std::string(mn ? "arc_fmin(" : "arc_fmax(") + a[0] + ", " + a[1] + ")");
        return bind(n->type, "(arc_str_cmp(" + a[0] + ", " + a[1] + ") " + (mn ? "<=" : ">=") + " 0 ? " + a[0] + " : " + a[1] + ")");
    }
    case BI::Sqrt: return bind(T.floatT, "sqrt(" + a[0] + ")");
    case BI::Floor: return bind(T.floatT, "floor(" + a[0] + ")");
    case BI::Ceil: return bind(T.floatT, "ceil(" + a[0] + ")");
    case BI::Round: return bind(T.floatT, "round(" + a[0] + ")");
    case BI::Sin: return bind(T.floatT, "sin(" + a[0] + ")");
    case BI::Cos: return bind(T.floatT, "cos(" + a[0] + ")");
    case BI::Tan: return bind(T.floatT, "tan(" + a[0] + ")");
    case BI::Exp: return bind(T.floatT, "exp(" + a[0] + ")");
    case BI::Log: return bind(T.floatT, "log(" + a[0] + ")");
    case BI::Log10: return bind(T.floatT, "log10(" + a[0] + ")");
    case BI::Pow: return bind(T.floatT, "pow(" + a[0] + ", " + a[1] + ")");
    case BI::Atan2: return bind(T.floatT, "atan2(" + a[0] + ", " + a[1] + ")");
    case BI::IPow: return bind(T.intT, "arc_ipow(" + a[0] + ", " + a[1] + ", " + L + ")");
    case BI::Assert: {
        if (a.size() == 2) line("if (ARC_UNLIKELY(!(" + a[0] + "))) arc_panic_at(" + L + ", \"%s\", " + a[1] + "->data);");
        else line("if (ARC_UNLIKELY(!(" + a[0] + "))) arc_panic_at(" + L + ", \"assertion failed: %s\", " + cstrLit(srcText(n->kids[0]->loc)) + ");");
        return "";
    }
    case BI::AssertEq: {
        Type* t = ty(0);
        std::string e = eqExpr(t, a[0], a[1]);
        open("if (ARC_UNLIKELY(!" + e + "))");
        line("ArcBuf __l, __r;");
        line("arc_buf_init(&__l);");
        line("arc_buf_init(&__r);");
        fmtInto("__l", a[0], t, "", true);
        fmtInto("__r", a[1], t, "", true);
        line("arc_buf_putc(&__l, 0);");
        line("arc_buf_putc(&__r, 0);");
        line("arc_panic_at(" + L + ", \"assertion `left == right` failed\\n  left: %s\\n right: %s\", __l.p, __r.p);");
        close();
        return "";
    }
    default: break;
    }
    // remaining builtins
    switch (n->builtin) {
    case BI::Panic:
        if (a.empty()) line("arc_panic_at(" + L + ", \"explicit panic\");");
        else line("arc_panic_at(" + L + ", \"%s\", " + a[0] + "->data);");
        return "";
    case BI::Exit: line("arc_exit(" + a[0] + ");"); return "";
    case BI::ReadLine: return bind(n->type, "arc_read_line()");
    case BI::ReadFile: return bind(n->type, "arc_read_file(" + a[0] + ")");
    case BI::WriteFile: return bind(T.boolT, "arc_write_file(" + a[0] + ", " + a[1] + ", 0)");
    case BI::AppendFile: return bind(T.boolT, "arc_write_file(" + a[0] + ", " + a[1] + ", 1)");
    case BI::FileExists: return bind(T.boolT, "arc_file_exists(" + a[0] + ")");
    case BI::Getenv: return bind(n->type, "arc_getenv(" + a[0] + ")");
    case BI::RunCmd: return bind(T.intT, "arc_run_cmd(" + a[0] + ")");
    case BI::Args: return bind(n->type, "arc_arr_slice(arc_args_arr, 0, arc_args_arr->len, " + L + ")");
    case BI::TimeNs: return bind(T.intT, "arc_time_ns()");
    case BI::TimeMs: return bind(T.intT, "(arc_time_ns() / 1000000)");
    case BI::UnixTime: return bind(T.intT, "arc_unix_time()");
    case BI::GcCount: return bind(T.intT, "((int64_t)arc_gc_count)");
    case BI::SleepMs: line("arc_sleep_ms(" + a[0] + ");"); return "";
    case BI::RandInt: return bind(T.intT, "arc_rand_int(" + a[0] + ", " + a[1] + ", " + L + ")");
    case BI::RandFloat: return bind(T.floatT, "arc_rand_float()");
    case BI::Seed: line("arc_seed(" + a[0] + ");"); return "";
    case BI::Chr: return bind(T.strT, "arc_chr(" + a[0] + ", " + L + ")");
    case BI::Err: {
        Type* rt = n->type;
        if (rt->kind != TK::Result) return bind(T.strT, a[0]);  // unreachable: the checker requires a `!T` context
        return bind(rt, resFromErr(rt, "arc_err_new(" + a[0] + ")"));
    }
    case BI::Ok:
        if (a.empty()) return "((ArcErr *)NULL)";
        return a[0];
    case BI::Exec: return bind(T.intT, "arc_exec(" + a[0] + ", " + a[1] + ")");
    case BI::ListDir: return bind(n->type, "arc_list_dir(" + a[0] + ")");
    case BI::MakeDir: return bind(n->type, "arc_make_dir(" + a[0] + ")");
    case BI::RemoveFile: return bind(n->type, "arc_remove_file(" + a[0] + ")");
    case BI::Ord: return bind(T.intT, "arc_ord(" + a[0] + ", " + L + ")");
    default: break;
    }
    line("/* unsupported builtin */");
    return "0";
}

std::string CGen::builtinMethod(Node* n) {
    Type* rt = n->kids[0]->type;
    std::vector<std::string> a;
    for (Node* k : n->kids) a.push_back(expr(k));
    std::string r = a[0];
    std::string L = loc(n->loc);
    Type* E = rt->elem;
    auto elemAt = [&](const std::string& idx) { return "((" + ctype(E) + " *)" + r + "->data)[" + idx + "]"; };
    switch (n->builtin) {
    // strings
    case BI::SLen: return bind(T.intT, r + "->len");
    case BI::SIsEmpty: return bind(T.boolT, "(" + r + "->len == 0)");
    case BI::SContains: return bind(T.boolT, "arc_str_contains(" + r + ", " + a[1] + ")");
    case BI::SStartsWith: return bind(T.boolT, "arc_str_starts_with(" + r + ", " + a[1] + ")");
    case BI::SEndsWith: return bind(T.boolT, "arc_str_ends_with(" + r + ", " + a[1] + ")");
    case BI::SFind: return bind(T.intT, "arc_str_find(" + r + ", " + a[1] + ")");
    case BI::SUpper: return bind(T.strT, "arc_str_upper(" + r + ")");
    case BI::SLower: return bind(T.strT, "arc_str_lower(" + r + ")");
    case BI::STrim: return bind(T.strT, "arc_str_trim(" + r + ")");
    case BI::SSplit: return bind(n->type, "arc_str_split(" + r + ", " + a[1] + ")");
    case BI::SSplitWs: return bind(n->type, "arc_str_split_ws(" + r + ")");
    case BI::SLines: return bind(n->type, "arc_str_lines(" + r + ")");
    case BI::SReplace: return bind(T.strT, "arc_str_replace(" + r + ", " + a[1] + ", " + a[2] + ")");
    case BI::SRepeat: return bind(T.strT, "arc_str_repeat(" + r + ", " + a[1] + ", " + L + ")");
    case BI::SSlice: return bind(T.strT, "arc_str_slice(" + r + ", " + a[1] + ", " + a[2] + ", " + L + ")");
    case BI::SByteAt: return bind(T.intT, "arc_str_byte_at(" + r + ", " + a[1] + ", " + L + ")");
    case BI::SChars: return bind(n->type, "arc_str_chars(" + r + ")");
    case BI::SCharCount: return bind(T.intT, "arc_str_char_count(" + r + ")");
    case BI::SToInt: return bind(n->type, "arc_parse_int(" + r + ")");
    case BI::SToFloat: return bind(n->type, "arc_parse_float(" + r + ")");
    // arrays
    case BI::ALen: return bind(T.intT, r + "->len");
    case BI::AIsEmpty: return bind(T.boolT, "(" + r + "->len == 0)");
    case BI::APush: line("ARC_PUSH(" + ctype(E) + ", " + r + ", " + a[1] + ");"); return "";
    case BI::APop: return bind(E, elemAt("arc_pop_idx(" + r + ", " + L + ")"));
    case BI::AInsert: {
        std::string k = bind(T.intT, "arc_arr_insert_at(" + r + ", " + a[1] + ", " + L + ")");
        line(elemAt(k) + " = " + a[2] + ";");
        return "";
    }
    case BI::ARemove: {
        std::string t = newTemp(E);
        line("arc_arr_remove_at(" + r + ", " + a[1] + ", &" + t + ", " + L + ");");
        return t;
    }
    case BI::AClear: line("arc_arr_clear(" + r + ");"); return "";
    case BI::AContains: case BI::AIndexOf: {
        std::string v = lval(a[1], E);
        std::string call = "arc_arr_index_of(" + r + ", &" + v + ")";
        return bind(n->builtin == BI::AContains ? T.boolT : T.intT, n->builtin == BI::AContains ? "(" + call + " >= 0)" : call);
    }
    case BI::AReverse: line("arc_arr_reverse(" + r + ");"); return "";
    case BI::ASort: line("arc_arr_sort(" + r + ");"); return "";
    case BI::AJoin: return bind(T.strT, "arc_arr_join(" + r + ", " + a[1] + ")");
    case BI::ASlice: return bind(rt, "arc_arr_slice(" + r + ", " + a[1] + ", " + a[2] + ", " + L + ")");
    case BI::ACopy: return bind(rt, "arc_arr_slice(" + r + ", 0, " + r + "->len, " + L + ")");
    case BI::ALast: return bind(E, elemAt("arc_last_idx(" + r + ", " + L + ")"));
    case BI::AFirst: return bind(E, elemAt("arc_idx(" + r + ", 0, " + L + ")"));
    // maps
    case BI::MLen: return bind(T.intT, r + "->len");
    case BI::MIsEmpty: return bind(T.boolT, "(" + r + "->len == 0)");
    case BI::MContains: {
        std::string k = lval(a[1], rt->key);
        return bind(T.boolT, "(arc_map_get(" + r + ", &" + k + ") != NULL)");
    }
    case BI::MGet: case BI::MGetOr: {
        std::string k = lval(a[1], rt->key);
        std::string vt = ctype(rt->elem);
        std::string p = "mp" + std::to_string(++tmpCounter_);
        std::string t = newTemp(n->type);
        line("{ void *" + p + " = arc_map_get(" + r + ", &" + k + ");");
        if (n->builtin == BI::MGet)
            line("  " + t + " = " + p + " ? " + someValue(n->type, "*(" + vt + " *)" + p) + " : " + nilValue(n->type) + "; }");
        else
            line("  " + t + " = " + p + " ? *(" + vt + " *)" + p + " : " + a[2] + "; }");
        return t;
    }
    case BI::MRemove: {
        std::string k = lval(a[1], rt->key);
        return bind(T.boolT, "arc_map_remove(" + r + ", &" + k + ")");
    }
    case BI::MKeys: return bind(n->type, "arc_map_keys(" + r + ", " + td(n->type) + ")");
    case BI::MValues: return bind(n->type, "arc_map_values(" + r + ", " + td(n->type) + ")");
    case BI::MClear: line("arc_map_clear(" + r + ");"); return "";
    // results
    case BI::RIsOk: case BI::RIsErr: {
        std::string o = lval(r, rt);
        std::string e = resIsErr(rt, o);
        return bind(T.boolT, n->builtin == BI::RIsErr ? e : "(!" + e + ")");
    }
    case BI::RUnwrap: {
        std::string o = lval(r, rt);
        if (a.size() == 2)
            line("if (ARC_UNLIKELY(" + resIsErr(rt, o) + ")) arc_panic_at(" + L + ", \"%s: %s\", " + a[1] + "->data, " + resErr(rt, o) +
                 "->msg->data);");
        else
            line("if (ARC_UNLIKELY(" + resIsErr(rt, o) + ")) arc_unwrap_err_fail(" + resErr(rt, o) + ", " + L + ");");
        if (E->kind == TK::Void) return "";
        return bind(E, resVal(rt, o));
    }
    case BI::RUnwrapOr: {
        std::string o = lval(r, rt);
        return bind(E, "(" + resIsErr(rt, o) + " ? " + a[1] + " : " + resVal(rt, o) + ")");
    }
    case BI::RError: {
        std::string o = lval(r, rt);
        return bind(n->type, "(" + resIsErr(rt, o) + " ? " + resErr(rt, o) + "->msg : NULL)");
    }
    // higher-order array methods (inline loops calling the closure)
    case BI::AMap: case BI::AFilter: case BI::AAny: case BI::AAll: case BI::AFind: {
        Type* ft = n->kids[1]->type;
        std::string src = bind(rt, r);
        std::string f = lval(a[1], ft);
        std::string res;
        switch (n->builtin) {
        case BI::AMap: res = bind(n->type, "arc_arr_new(" + td(n->type) + ", 0, " + src + "->len)"); break;
        case BI::AFilter: res = bind(n->type, "arc_arr_new(" + td(n->type) + ", 0, 0)"); break;
        case BI::AAny: res = bind(T.boolT, "false"); break;
        case BI::AAll: res = bind(T.boolT, "true"); break;
        default: res = bind(n->type, nilValue(n->type)); break;
        }
        std::string k = "k" + std::to_string(++tmpCounter_);
        open("for (int64_t " + k + " = 0; " + k + " < " + src + "->len; " + k + "++)");
        std::string x = bind(E, "((" + ctype(E) + " *)" + src + "->data)[" + k + "]");
        if (n->builtin == BI::AMap) {
            Type* U = n->type->elem;
            std::string y = bind(U, callClosure(ft, f, {x}));
            line("ARC_PUSH(" + ctype(U) + ", " + res + ", " + y + ");");
        } else {
            std::string c = bind(T.boolT, callClosure(ft, f, {x}));
            if (n->builtin == BI::AFilter) line("if (" + c + ") ARC_PUSH(" + ctype(E) + ", " + res + ", " + x + ");");
            else if (n->builtin == BI::AAny) line("if (" + c + ") { " + res + " = true; break; }");
            else if (n->builtin == BI::AAll) line("if (!" + c + ") { " + res + " = false; break; }");
            else line("if (" + c + ") { " + res + " = " + someValue(n->type, x) + "; break; }");
        }
        close();
        return res;
    }
    case BI::AFold: {
        Type* ft = n->kids[2]->type;
        std::string src = bind(rt, r);
        std::string f = lval(a[2], ft);
        std::string acc = bind(n->type, a[1]);
        std::string k = "k" + std::to_string(++tmpCounter_);
        open("for (int64_t " + k + " = 0; " + k + " < " + src + "->len; " + k + "++)");
        std::string x = bind(E, "((" + ctype(E) + " *)" + src + "->data)[" + k + "]");
        line(acc + " = " + callClosure(ft, f, {acc, x}) + ";");
        close();
        return acc;
    }
    case BI::ASortBy: {
        Type* ft = n->kids[1]->type;
        std::string f = lval(a[1], ft);
        std::string cmp = "arc_less_" + std::to_string(++lessCounter_);
        std::string et = ctype(E);
        extraFns_ << "static bool " << cmp << "(ArcClo *f, const void *a, const void *b) {\n    return "
                  << callClosure(ft, "f", {"*(" + et + " *)a", "*(" + et + " *)b"}) << ";\n}\n\n";
        extraProtos_ << "static bool " << cmp << "(ArcClo *f, const void *a, const void *b);\n";
        line("arc_arr_sort_by(" + r + ", " + f + ", " + cmp + ");");
        return "";
    }
    case BI::ASum: {
        std::string src = bind(rt, r);
        std::string acc = bind(E, E->kind == TK::Int ? "((int64_t)0)" : "(0.0)");
        std::string k = "k" + std::to_string(++tmpCounter_);
        line("for (int64_t " + k + " = 0; " + k + " < " + src + "->len; " + k + "++) " + acc + " += ((" + ctype(E) + " *)" + src +
             "->data)[" + k + "];");
        return acc;
    }
    case BI::AMin: case BI::AMax: {
        std::string src = bind(rt, r);
        std::string res = bind(n->type, nilValue(n->type));
        std::string k = "k" + std::to_string(++tmpCounter_);
        open("for (int64_t " + k + " = 0; " + k + " < " + src + "->len; " + k + "++)");
        std::string x = bind(E, "((" + ctype(E) + " *)" + src + "->data)[" + k + "]");
        bool heapOpt = E->isHeap();
        std::string has = heapOpt ? "(" + res + " != NULL)" : res + ".has";
        std::string cur = heapOpt ? res : res + ".val";
        std::string op = n->builtin == BI::AMin ? "<" : ">";
        std::string better = E->kind == TK::Str ? "(arc_str_cmp(" + x + ", " + cur + ") " + op + " 0)" : "(" + x + " " + op + " " + cur + ")";
        line("if (!" + has + " || " + better + ") " + res + " = " + someValue(n->type, x) + ";");
        close();
        return res;
    }
    // optionals
    case BI::OUnwrap: {
        std::string o = lval(r, rt);
        std::string isNil = E->isHeap() ? o + " == NULL" : "!" + o + ".has";
        if (a.size() == 2) line("if (ARC_UNLIKELY(" + isNil + ")) arc_panic_at(" + L + ", \"%s\", " + a[1] + "->data);");
        else line("if (ARC_UNLIKELY(" + isNil + ")) arc_unwrap_fail(" + L + ");");
        return bind(E, E->isHeap() ? o : o + ".val");
    }
    case BI::OIsSome: case BI::OIsNil: {
        std::string o = lval(r, rt);
        std::string some = E->isHeap() ? "(" + o + " != NULL)" : o + ".has";
        return bind(T.boolT, n->builtin == BI::OIsSome ? some : "(!" + some + ")");
    }
    case BI::OUnwrapOr: {
        std::string o = lval(r, rt);
        if (E->isHeap()) return bind(E, "(" + o + " ? " + o + " : " + a[1] + ")");
        return bind(E, "(" + o + ".has ? " + o + ".val : " + a[1] + ")");
    }
    default: break;
    }
    line("/* unsupported method */");
    return "0";
}

// ----------------------------------------------------------------- module --

std::string CGen::generate(const std::string& runtimeSrc) {
    // make sure every struct has a descriptor
    for (StructDecl* sd : prog_.structs) td(sd->type);

    // closures read captured values through their environment object
    for (FnDecl* L : prog_.lambdas)
        for (size_t i = 0; i < L->captures.size(); i++)
            L->captures[i].second->cname = "((ArcL_" + std::to_string(L->lambdaId) + " *)env_)->c_" + std::to_string(i);

    std::ostringstream fns;
    std::vector<FnDecl*> all;
    for (FnDecl* fn : prog_.fns) all.push_back(fn);
    for (FnDecl* fn : prog_.methods) all.push_back(fn);
    for (FnDecl* fn : prog_.lambdas) all.push_back(fn);
    // assign parameter names before prototypes are printed
    for (FnDecl* fn : all) for (auto& p : fn->params) if (p.var) varName(p.var);
    for (FnDecl* fn : all) fns << genFunction(fn);
    std::string init = genGlobalInit();

    // struct / enum / closure-environment layouts (rendered first: they may register result types)
    std::ostringstream defs;
    for (StructDecl* sd : prog_.structs) {
        defs << "struct " << structName(sd) << " {\n    ArcObj h;\n";
        if (sd->isEnum) defs << "    int64_t tag;\n";
        for (auto& f : sd->fields) defs << "    " << ctype(f.type) << " f_" << mangle(f.name) << ";\n";
        defs << "};\n";
    }
    for (FnDecl* L : prog_.lambdas) {
        defs << "typedef struct {\n    ArcObj h;\n    void *fn;\n";
        for (size_t i = 0; i < L->captures.size(); i++) defs << "    " << ctype(L->captures[i].second->type) << " c_" << i << ";\n";
        defs << "} ArcL_" << L->lambdaId << ";\n";
        for (auto& c : L->captures) td(c.second->type);
    }
    for (FnDecl* f : fnValues_) for (Type* p : f->paramTypes) ctype(p);
    for (size_t i = 0; i < tdOrder_.size(); i++) ctype(tdOrder_[i]);  // registers result typedefs used only in descriptors

    std::ostringstream o;
    o << runtimeSrc << "\n";
    for (StructDecl* sd : prog_.structs) o << "typedef struct " << structName(sd) << " " << structName(sd) << ";\n";
    for (auto& kv : resTypes_)  // value results: {error, value}
        o << "typedef struct { ArcErr *err; " << ctype(kv.second->elem) << " val; } ArcRes_" << kv.first << ";\n";
    o << defs.str();
    // descriptors (generated in dependency-free order thanks to forward declarations)
    for (Type* t : tdOrder_) o << "static const ArcTD " << tdNames_[t->id] << ";\n";
    for (Type* t : tdOrder_) {
        std::string name = tdNames_[t->id];
        if (t->kind == TK::Struct) {
            StructDecl* sd = t->sd;
            if (!sd->fields.empty()) {
                o << "static const ArcField " << name << "_f[] = {";
                for (size_t i = 0; i < sd->fields.size(); i++) {
                    auto& f = sd->fields[i];
                    o << (i ? ", " : "") << "{" << cstrLit(f.name) << ", offsetof(" << structName(sd) << ", f_" << mangle(f.name)
                      << "), " << td(f.type) << "}";
                }
                o << "};\n";
            }
            o << "static const ArcTD " << name << " = {ARC_K_STRUCT, 1, sizeof(void *), 0, 0, " << cstrLit(sd->name) << ", "
              << sd->fields.size() << ", " << (sd->fields.empty() ? "0" : name + "_f") << ", sizeof(" << structName(sd) << "), 0};\n";
        } else if (t->kind == TK::Array) {
            o << "static const ArcTD " << name << " = {ARC_K_ARR, 1, sizeof(void *), " << td(t->elem) << ", 0, 0, 0, 0, 0, 0};\n";
        } else if (t->kind == TK::Map) {
            o << "static const ArcTD " << name << " = {ARC_K_MAP, 1, sizeof(void *), " << td(t->elem) << ", " << td(t->key)
              << ", 0, 0, 0, 0, 0};\n";
        } else if (t->kind == TK::Optional) {
            // optional of a heap type: nullable pointer
            o << "static const ArcTD " << name << " = {ARC_K_OPT, 1, sizeof(void *), " << td(t->elem) << ", 0, 0, 0, 0, 0, 0};\n";
        } else if (t->kind == TK::Enum) {
            StructDecl* sd = t->sd;
            if (!sd->fields.empty()) {
                o << "static const ArcField " << name << "_f[] = {";
                for (size_t i = 0; i < sd->fields.size(); i++)
                    o << (i ? ", " : "") << "{" << cstrLit(sd->fields[i].name) << ", offsetof(" << structName(sd) << ", f_"
                      << mangle(sd->fields[i].name) << "), " << td(sd->fields[i].type) << "}";
                o << "};\n";
            }
            o << "static const char *const " << name << "_vn[] = {";
            for (size_t i = 0; i < sd->variants.size(); i++) o << (i ? ", " : "") << cstrLit(sd->variants[i].name);
            o << "};\nstatic const int " << name << "_vf[] = {";
            for (size_t i = 0; i < sd->variants.size(); i++) o << (i ? ", " : "") << sd->variants[i].firstField;
            o << "};\nstatic const int " << name << "_vc[] = {";
            for (size_t i = 0; i < sd->variants.size(); i++) o << (i ? ", " : "") << sd->variants[i].types.size();
            o << "};\n";
            o << "static const ArcTD " << name << " = {ARC_K_ENUM, 1, sizeof(void *), 0, 0, " << cstrLit(sd->name) << ", "
              << sd->fields.size() << ", " << (sd->fields.empty() ? "0" : name + "_f") << ", sizeof(" << structName(sd) << "), 0, "
              << sd->variants.size() << ", " << name << "_vn, " << name << "_vf, " << name << "_vc};\n";
        } else if (t->kind == TK::Result) {
            std::string et = t->elem->kind == TK::Void ? "0" : td(t->elem);
            if (t->isHeap())
                o << "static const ArcTD " << name << " = {ARC_K_RES, 1, sizeof(void *), " << et << ", 0, 0, 0, 0, 0, 0};\n";
            else
                o << "static const ArcTD " << name << " = {ARC_K_RES, 0, sizeof(ArcRes_" << t->id << "), " << et
                  << ", 0, 0, 0, 0, 0, offsetof(ArcRes_" << t->id << ", val)};\n";
        }
    }
    // closure environments: scanned like structs
    for (FnDecl* L : prog_.lambdas) {
        std::string id = std::to_string(L->lambdaId);
        if (!L->captures.empty()) {
            o << "static const ArcField arc_td_lam_" << id << "_f[] = {";
            for (size_t i = 0; i < L->captures.size(); i++)
                o << (i ? ", " : "") << "{\"" << L->captures[i].second->name << "\", offsetof(ArcL_" << id << ", c_" << i << "), "
                  << td(L->captures[i].second->type) << "}";
            o << "};\n";
        }
        o << "static const ArcTD arc_td_lam_" << id << " = {ARC_K_STRUCT, 1, sizeof(void *), 0, 0, \"closure\", "
          << L->captures.size() << ", " << (L->captures.empty() ? "0" : "arc_td_lam_" + id + "_f") << ", sizeof(ArcL_" << id
          << "), 0};\n";
    }
    // payload-free enum variants are shared static objects
    for (StructDecl* sd : prog_.structs) {
        if (!sd->isEnum) continue;
        for (size_t i = 0; i < sd->variants.size(); i++)
            if (sd->variants[i].types.empty())
                o << "static " << structName(sd) << " arc_ev_" << mangle(sd->name) << "_" << i << " = {{0, " << td(sd->type)
                  << ", ARC_F_STATIC, 0}, " << i << "};\n";
    }
    // string literals
    for (size_t i = 0; i < strLitOrder_.size(); i++) {
        const std::string& s = strLitOrder_[i];
        o << "static ArcStr arc_lit_" << i << " = {{0, &arc_td_str, ARC_F_STATIC, 0}, " << s.size() << ", " << cstrLit(s) << "};\n";
    }
    // file names
    o << "static const char *const arc_files[] = {";
    for (size_t i = 0; i < prog_.files.size(); i++) o << (i ? ", " : "") << cstrLit(prog_.files[i]->path);
    if (prog_.files.empty()) o << "\"?\"";
    o << "};\n";
    // globals
    for (Node* g : prog_.globals)
        if (g->var) o << "static " << ctype(g->var->type) << " " << varName(g->var) << ";\n";
    // prototypes
    for (FnDecl* fn : all) o << prototype(fn) << ";\n";
    o << extraProtos_.str();
    // named functions used as values: a trampoline plus a static closure object
    for (FnDecl* f : fnValues_) {
        std::string tr = "arc_tr_" + fnName(f);
        std::string sig = "static " + ctype(f->ret) + " " + tr + "(ArcClo *env_";
        std::string call = fnName(f) + "(";
        for (size_t i = 0; i < f->paramTypes.size(); i++) {
            sig += ", " + ctype(f->paramTypes[i]) + " p" + std::to_string(i);
            call += (i ? ", p" : "p") + std::to_string(i);
        }
        call += ")";
        o << sig << ") {\n    (void)env_;\n    " << (f->ret->kind == TK::Void ? "" : "return ") << call << ";\n}\n";
        o << "static ArcClo arc_fv_" << fnName(f) << " = {{0, &arc_td_fn, ARC_F_STATIC, 0}, (void *)" << tr << "};\n";
    }
    o << "\n" << fns.str() << extraFns_.str() << init;

    // tests
    std::vector<FnDecl*> tests;
    for (FnDecl* fn : prog_.fns) if (fn->isTest) tests.push_back(fn);
    if (testMode_) {
        o << "static const ArcTest arc_tests[] = {";
        for (size_t i = 0; i < tests.size(); i++) o << (i ? ", " : "") << "{" << cstrLit(tests[i]->name) << ", " << fnName(tests[i]) << "}";
        if (tests.empty()) o << "{0, 0}";
        o << "};\n";
    }

    o << "int main(int argc, char **argv) {\n";
    o << "    arc_runtime_init(argc, argv, arc_files);\n";
    for (Node* g : prog_.globals)
        if (g->var && g->var->type->isHeap()) o << "    arc_add_groot((void **)&" << varName(g->var) << ");\n";
    o << "    arc_init_globals();\n";
    if (testMode_) {
        o << "    int rc = arc_run_tests(arc_tests, " << tests.size() << ", argc > 1 ? argv[1] : NULL);\n";
        o << "    arc_exit(rc);\n";
    } else if (prog_.mainFn && prog_.mainFn->ret->kind == TK::Int) {
        o << "    arc_exit(arc_fn_main());\n";
    } else if (prog_.mainFn && prog_.mainFn->ret->kind == TK::Result) {
        o << "    arc_exit(arc_main_result(arc_fn_main()));\n";
    } else {
        o << "    arc_fn_main();\n    arc_exit(0);\n";
    }
    o << "}\n";
    return o.str();
}

}  // namespace arc
