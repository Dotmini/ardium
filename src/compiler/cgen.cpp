#include "cgen.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace arc {

// ---------------------------------------------------------------- naming --

std::string CGen::mangle(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '_') r += (char)c;
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
    if (fn->owner) return "arc_m_" + mangle(fn->owner->name) + "_" + mangle(fn->name);
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
    case TK::Struct: return structName(t->sd) + " *";
    case TK::Void: return "void";
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
    if (t->elem) td(t->elem);
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

std::string CGen::prototype(FnDecl* fn) {
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
    for (Var* v : fnVars_) {
        std::string ct = ctype(v->type);
        if (v->type->isHeap()) {
            o << "    " << ct << " " << varName(v) << " = NULL;\n";
            roots.push_back(varName(v));
        } else {
            o << "    " << ct << " " << varName(v) << ";\n";
        }
    }
    for (auto& t : temps_) {
        if (t.type->kind == TK::Void) continue;
        std::string ct = ctype(t.type);
        if (t.type->isHeap()) {
            o << "    " << ct << " " << t.name << " = NULL;\n";
            roots.push_back(t.name);
        } else {
            o << "    " << ct << " " << t.name << ";\n";
        }
    }
    if (fn_) {
        if (fn_->hasSelf) roots.push_back("a_self");
        for (size_t i = 0; i < fn_->params.size(); i++)
            if (fn_->paramTypes[i]->isHeap()) roots.push_back(varName(fn_->params[i].var));
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
    o << "}\n\n";
    return o.str();
}

static bool needsFrame(const std::vector<Var*>& vars, const std::vector<std::pair<std::string, Type*>>& extra) {
    for (Var* v : vars) if (v->type->isHeap()) return true;
    for (auto& e : extra) if (e.second->isHeap()) return true;
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
    blockStmts(fn->body);
    bool anyHeapTemp = false;
    for (auto& t : temps_) if (t.type->isHeap()) anyHeapTemp = true;
    bool need = anyHeapTemp || needsFrame(fnVars_, params);
    if (!need) {
        // regenerate without frame bookkeeping
        hasFrame_ = false;
        temps_.clear();
    lastBindName_.clear();
        tmpCounter_ = 0;
        body.str("");
        body.clear();
        blockStmts(fn->body);
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
    for (auto& t : temps_) if (t.type->isHeap()) anyHeap = true;
    hasFrame_ = anyHeap;
    if (!anyHeap) {
        // no GC temps: plain body is fine (frame lines were never emitted inline)
    }
    return assembleBody("static void arc_init_globals(void)", body.str(), T.voidT);
}

// ------------------------------------------------------------ statements --

void CGen::blockStmts(Node* n) {
    for (Node* s : n->kids) stmt(s);
}

void CGen::stmt(Node* n) {
    switch (n->kind) {
    case NK::Let: {
        std::string v = expr(n->kids[0]);
        if (!storeDirect(v, varName(n->var))) line(varName(n->var) + " = " + v + ";");
        return;
    }
    case NK::Assign: assign(n); return;
    case NK::ExprStmt: expr(n->kids[0]); return;
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
        std::string cond = ot->elem->isHeap() ? "(" + o + ") != NULL" : "(" + o + ").has";
        open("if (" + cond + ")");
        line(varName(n->var) + " = " + (ot->elem->isHeap() ? o : "(" + o + ").val") + ";");
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
        open("for (int64_t " + k + " = " + s + "; " + k + (n->flag ? " <= " : " < ") + e + "; " + k + "++)");
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
        if (n->kids.empty()) { line(retStmt("")); return; }
        std::string v = expr(n->kids[0]);
        if (fn_ && fn_->ret->kind == TK::Void) { line(retStmt("")); return; }
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
        if (n->type->elem->isHeap()) return v;
        return bind(n->type, someValue(n->type, v));
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
        if (n->flag && n->kids[2]) b = "(" + b + " + 1)";
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
        std::string f = fill == '\'' ? "'\\''" : fill == '\\' ? "'\\\\'" : std::string("'") + fill + "'";
        line("arc_buf_align(" + B + ", " + st + ", " + std::to_string(width) + ", '" + align + "', " + f + ");");
    }
}

std::string CGen::callExpr(Node* n) {
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

    std::ostringstream fns;
    std::vector<FnDecl*> all;
    for (FnDecl* fn : prog_.fns) all.push_back(fn);
    for (FnDecl* fn : prog_.methods) all.push_back(fn);
    // assign parameter names before prototypes are printed
    for (FnDecl* fn : all) for (auto& p : fn->params) if (p.var) varName(p.var);
    for (FnDecl* fn : all) fns << genFunction(fn);
    std::string init = genGlobalInit();

    std::ostringstream o;
    o << runtimeSrc << "\n";
    // struct types
    for (StructDecl* sd : prog_.structs) o << "typedef struct " << structName(sd) << " " << structName(sd) << ";\n";
    for (StructDecl* sd : prog_.structs) {
        o << "struct " << structName(sd) << " {\n    ArcObj h;\n";
        for (auto& f : sd->fields) o << "    " << ctype(f.type) << " f_" << mangle(f.name) << ";\n";
        o << "};\n";
    }
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
        }
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
    o << "\n" << fns.str() << init;

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
    } else {
        o << "    arc_fn_main();\n    arc_exit(0);\n";
    }
    o << "}\n";
    return o.str();
}

}  // namespace arc
