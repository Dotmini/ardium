#include "parser.h"

#include <cctype>
#include <set>

namespace arc {

Parser::Parser(SourceFile* f, Diagnostics& d, Program& prog) : file_(f), diag_(d), prog_(prog) {
    Lexer lx(f, d);
    toks_ = lx.tokenize();
}

Parser::Parser(SourceFile* f, Diagnostics& d, Program& prog, std::vector<Token> toks)
    : file_(f), diag_(d), prog_(prog), toks_(std::move(toks)) {}

// ---------------------------------------------------------------- helpers --

std::string Parser::describe(const Token& t) const {
    switch (t.kind) {
    case Tok::Eof: return "end of file";
    case Tok::Ident: return "identifier `" + t.text + "`";
    case Tok::Keyword: return "keyword `" + t.text + "`";
    case Tok::Int: case Tok::Float: return "number `" + t.text + "`";
    case Tok::Str: case Tok::FStr: return "string literal";
    case Tok::Punct: return "`" + t.text + "`";
    }
    return "token";
}

void Parser::fail(const Loc& loc, const std::string& msg, const std::string& label, const std::string& help) {
    Diag& d = diag_.error(loc, msg, label);
    if (!help.empty()) d.notes.push_back("help: " + help);
    throw CompileAbort{};
}

void Parser::unexpected(const std::string& what, const std::string& help) {
    fail(cur().loc, "expected " + what + ", found " + describe(cur()), "expected " + what, help);
}

Token Parser::expectP(const char* s, const std::string& context) {
    if (isP(s)) return next();
    std::string help;
    if (std::string(s) == "{" && isP(":")) help = "";
    unexpected(std::string("`") + s + "`" + (context.empty() ? "" : " " + context), help);
}

Token Parser::expectIdent(const std::string& context) {
    if (isIdent()) return next();
    if (cur().kind == Tok::Keyword)
        fail(cur().loc, "expected " + context + ", found keyword `" + cur().text + "`", "keywords cannot be used as names");
    unexpected(context);
}

Loc Parser::prevEnd() const {
    if (p_ == 0) return cur().loc;
    Loc l = toks_[p_ - 1].loc;
    return l;
}

Loc Parser::span(const Loc& a, const Loc& b) const {
    Loc r = a;
    if (b.line == a.line && b.off + b.len >= a.off) r.len = b.off + b.len - a.off;
    return r;
}

// Nesting limit: keeps pathological input from overflowing the compiler's stack.
static const int kMaxNesting = 1000;
static const uint32_t kMaxChain = 20000;

void Parser::enter(const Loc& at) {
    if (++depth_ > kMaxNesting)
        fail(at, "code is nested too deeply (more than " + std::to_string(kMaxNesting) + " levels)", "",
             "split the expression or block into smaller functions");
}

void Parser::endStmt() {
    if (acceptP(";")) { while (acceptP(";")) {} return; }
    if (armStmt_ && isP(",")) return;
    if (isP("}") || cur().kind == Tok::Eof || cur().nl) return;
    Loc at = prevEnd();
    at.off += at.len; at.col += at.len; at.len = 1;
    fail(cur().loc, "expected `;` or a newline after statement, found " + describe(cur()), "unexpected token",
         "put each statement on its own line or separate statements with `;`");
}

// ------------------------------------------------------------------ items --

std::vector<ImportReq> Parser::parseFile() {
    std::vector<ImportReq> imports;
    while (cur().kind != Tok::Eof) {
        if (acceptP(";")) continue;
        parseItem(imports);
    }
    return imports;
}

void Parser::parseItem(std::vector<ImportReq>& imports) {
    std::vector<std::string> attrs;
    while (isP("@")) {
        next();
        Token a = expectIdent("attribute name after `@`");
        attrs.push_back(a.text);
        if (acceptP("(")) {  // ignore attribute arguments
            int depth = 1;
            while (depth > 0 && cur().kind != Tok::Eof) {
                if (isP("(")) depth++;
                if (isP(")")) depth--;
                next();
            }
        }
    }
    if (isKw("fn")) {
        FnDecl* fn = parseFn(attrs, nullptr, false);
        prog_.fns.push_back(fn);
        return;
    }
    if (!attrs.empty()) fail(cur().loc, "attributes are only allowed on functions", "", "");
    if (isKw("import")) {
        Token kw = next();
        std::string path;
        Loc loc = cur().loc;
        if (cur().kind == Tok::Str) path = next().text;
        else if (isIdent()) path = next().text;
        else unexpected("module name after `import`", "write `import \"utils\"` to import utils.ar");
        imports.push_back({path, span(kw.loc, loc)});
        endStmt();
        return;
    }
    if (isKw("struct")) { parseStruct(); return; }
    if (isKw("enum")) { parseEnum(); return; }
    if (isKw("impl")) { parseImpl(); return; }
    if (isKw("let") || isKw("var")) {
        Node* n = parseLet(true);
        prog_.globals.push_back(n);
        return;
    }
    if (isIdent() && cur().text == "extern")
        fail(cur().loc, "`extern` declarations are not supported by this compiler yet", "", "");
    if (isIdent() && cur().text == "class")
        fail(cur().loc, "`class` is not supported", "", "use `struct` with an `impl` block");
    fail(cur().loc, "expected an item (fn, struct, impl, let, import), found " + describe(cur()),
         "statements are not allowed at the top level",
         "move this code into `fn main() { ... }`");
}

FnDecl* Parser::parseFn(std::vector<std::string> attrs, StructDecl* owner, bool inImpl) {
    Token kw = next();  // fn
    Token name = expectIdent("function name after `fn`");
    FnDecl* fn = new FnDecl();
    fn->name = name.text;
    fn->loc = name.loc;
    fn->attrs = attrs;
    fn->owner = owner;
    for (auto& a : attrs) {
        if (a == "test") fn->isTest = true;
    }
    expectP("(", "after function name");
    bool first = true;
    while (!isP(")")) {
        if (!first) {
            expectP(",", "between parameters");
            if (isP(")")) break;
        }
        Param prm;
        bool amp = acceptP("&");
        bool mut = acceptKw("mut");
        if (isIdent() && cur().text == "self") {
            Token s = next();
            if (!first || !inImpl)
                fail(s.loc, inImpl ? "`self` must be the first parameter" : "`self` is only allowed in methods inside `impl` blocks");
            fn->hasSelf = true;
            fn->selfMut = mut;
            first = false;
            continue;
        }
        (void)amp;
        Token pn = expectIdent("parameter name");
        prm.name = pn.text;
        prm.loc = pn.loc;
        prm.mut = mut;
        if (!isP(":"))
            fail(pn.loc, "missing type for parameter `" + pn.text + "`", "needs a type",
                 "add a type annotation, e.g. `" + pn.text + ": int`");
        next();
        prm.te = parseType();
        fn->params.push_back(prm);
        first = false;
    }
    expectP(")", "to close the parameter list");
    if (acceptP("->")) fn->retTe = parseType();
    else if (isP(":") && peekTok().kind != Tok::Punct) {
        fail(cur().loc, "unexpected `:` before return type", "", "return types are written with `->`, e.g. `fn f() -> int`");
    }
    acceptP(":");  // legacy `fn main():`
    if (!isP("{")) unexpected("`{` to start the function body");
    fn->body = parseBlock();
    (void)kw;
    return fn;
}

void Parser::parseStruct() {
    next();  // struct
    Token name = expectIdent("struct name");
    StructDecl* sd = new StructDecl();
    sd->name = name.text;
    sd->loc = name.loc;
    acceptP(":");
    expectP("{", "to start the struct body");
    nest_++;
    while (!isP("}")) {
        if (acceptP(",") || acceptP(";")) continue;
        Token fname = expectIdent("field name");
        FieldDecl fd;
        fd.name = fname.text;
        fd.loc = fname.loc;
        if (!isP(":")) fail(fname.loc, "missing type for field `" + fname.text + "`", "", "write `" + fname.text + ": int`");
        next();
        fd.te = parseType();
        for (auto& f : sd->fields)
            if (f.name == fd.name) fail(fname.loc, "field `" + fd.name + "` is already declared");
        sd->fields.push_back(fd);
    }
    nest_--;
    expectP("}", "to close the struct");
    prog_.structs.push_back(sd);
}

void Parser::parseEnum() {
    next();  // enum
    Token name = expectIdent("enum name");
    StructDecl* sd = new StructDecl();
    sd->name = name.text;
    sd->loc = name.loc;
    sd->isEnum = true;
    acceptP(":");
    expectP("{", "to start the enum body");
    nest_++;
    while (!isP("}")) {
        if (acceptP(",") || acceptP(";")) continue;
        Token vname = expectIdent("variant name");
        Variant v;
        v.name = vname.text;
        v.loc = vname.loc;
        if (acceptP("(")) {
            while (!isP(")")) {
                if (!v.tes.empty()) expectP(",", "between payload types");
                if (isP(")")) break;
                v.tes.push_back(parseType());
            }
            expectP(")", "to close the payload types");
        } else if (isP("{")) {
            fail(cur().loc, "variants with named fields are not supported", "", "use positional payloads: `" + v.name + "(int, str)`");
        }
        if (sd->variantIndex(v.name) >= 0) fail(vname.loc, "variant `" + v.name + "` is declared more than once");
        sd->variants.push_back(v);
    }
    nest_--;
    expectP("}", "to close the enum");
    if (sd->variants.empty()) fail(name.loc, "enum `" + sd->name + "` has no variants");
    prog_.structs.push_back(sd);
}

void Parser::parseImpl() {
    Token kw = next();  // impl
    Token name = expectIdent("type name after `impl`");
    acceptP(":");
    expectP("{", "to start the impl block");
    // Owner is resolved later by the checker; stash the name in a placeholder struct.
    StructDecl* placeholder = new StructDecl();
    placeholder->name = name.text;
    placeholder->loc = name.loc;
    while (!isP("}")) {
        if (acceptP(";")) continue;
        std::vector<std::string> attrs;
        while (isP("@")) { next(); attrs.push_back(expectIdent("attribute name").text); }
        if (!isKw("fn")) unexpected("`fn` inside `impl` block");
        FnDecl* fn = parseFn(attrs, placeholder, true);
        prog_.methods.push_back(fn);
    }
    expectP("}", "to close the impl block");
    (void)kw;
}

TypeExpr* Parser::parseType() {
    TypeExpr* t = new TypeExpr();
    t->loc = cur().loc;
    if (acceptP("!")) {
        t->kind = TypeExpr::Result;
        if (isIdent() && cur().text == "void") {
            TypeExpr* v = new TypeExpr();
            v->loc = cur().loc;
            v->name = next().text;
            t->a = v;
        } else {
            t->a = parseType();
        }
        t->loc = span(t->loc, prevEnd());
        return t;
    }
    if (isKw("fn")) {
        next();
        t->kind = TypeExpr::Func;
        expectP("(", "after `fn` in a function type");
        while (!isP(")")) {
            if (!t->params.empty()) expectP(",", "between parameter types");
            if (isP(")")) break;
            t->params.push_back(parseType());
        }
        expectP(")", "to close the parameter types");
        if (acceptP("->")) t->b = parseType();
        t->loc = span(t->loc, prevEnd());
        return t;
    }
    if (acceptP("?")) {
        t->kind = TypeExpr::Optional;
        t->a = parseType();
        t->loc = span(t->loc, prevEnd());
        return t;
    }
    if (acceptP("[")) {
        TypeExpr* inner = parseType();
        if (acceptP(":")) {
            t->kind = TypeExpr::Map;
            t->a = inner;
            t->b = parseType();
        } else {
            t->kind = TypeExpr::Array;
            t->a = inner;
        }
        expectP("]", "to close the type");
        t->loc = span(t->loc, prevEnd());
        return t;
    }
    if (isIdent()) {
        t->kind = TypeExpr::Name;
        t->name = next().text;
        return t;
    }
    unexpected("a type", "types look like `int`, `float`, `bool`, `str`, `[int]`, `[str: int]`, `?int`, `!int`, `fn(int) -> int` or a struct/enum name");
}

// ------------------------------------------------------------- statements --

Node* Parser::parseBlock() {
    acceptP(":");
    enter(cur().loc);
    Token lb = expectP("{", "to start a block");
    Node* b = new Node(NK::Block, lb.loc);
    int savedNest = nest_;
    bool savedNoStruct = noStruct_;
    nest_ = 0;
    noStruct_ = false;
    while (!isP("}")) {
        if (cur().kind == Tok::Eof) fail(lb.loc, "unclosed block: this `{` has no matching `}`", "block starts here");
        if (acceptP(";")) continue;
        b->kids.push_back(parseStmt());
    }
    next();
    nest_ = savedNest;
    noStruct_ = savedNoStruct;
    leave();
    return b;
}

static const std::set<std::string> kAssignOps = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};

Node* Parser::parseStmt() {
    if (isKw("let") || isKw("var")) return parseLet(false);
    if (isKw("if")) return parseIf();
    if (isKw("match")) return parseMatch(false);
    if (isKw("while")) return parseWhile();
    if (isKw("for")) return parseFor();
    if (isKw("loop")) {
        Token kw = next();
        if (!isP("{") && !isP(":")) {  // legacy: loop (cond) { }
            bool saved = noStruct_;
            noStruct_ = true;
            Node* cond = parseExpr();
            noStruct_ = saved;
            Node* w = new Node(NK::While, kw.loc);
            w->kids = {cond, parseBlock()};
            return w;
        }
        Node* n = new Node(NK::Loop, kw.loc);
        n->kids.push_back(parseBlock());
        return n;
    }
    if (isKw("break") || isKw("continue")) {
        Token kw = next();
        Node* n = new Node(kw.text == "break" ? NK::Break : NK::Continue, kw.loc);
        endStmt();
        return n;
    }
    if (isKw("return")) {
        Token kw = next();
        Node* n = new Node(NK::Return, kw.loc);
        if (!isP(";") && !isP("}") && !cur().nl && cur().kind != Tok::Eof) n->kids.push_back(parseExpr());
        endStmt();
        return n;
    }
    if (isP("{")) return parseBlock();
    if (isKw("fn")) fail(cur().loc, "nested functions are not supported", "", "declare the function at the top level");
    if (isKw("struct") || isKw("impl")) fail(cur().loc, "`" + cur().text + "` must be declared at the top level");
    if (isKw("else") || isKw("elif")) fail(cur().loc, "`" + cur().text + "` without a matching `if`");

    Node* e = parseExpr();
    if (cur().kind == Tok::Punct && kAssignOps.count(cur().text)) {
        Token op = next();
        Node* rhs = parseExpr();
        Node* a = new Node(NK::Assign, op.loc);
        a->str = op.text;
        a->kids = {e, rhs};
        endStmt();
        return a;
    }
    if (isP("++") || (isP("+") && peekTok().kind == Tok::Punct && peekTok().text == "+"))
        fail(cur().loc, "Arc has no `++` operator", "", "use `x += 1`");
    Node* s = new Node(NK::ExprStmt, e->loc);
    s->kids.push_back(e);
    endStmt();
    return s;
}

Node* Parser::parseLet(bool global) {
    Token kw = next();
    bool mut = kw.text == "var";
    if (acceptKw("mut")) mut = true;
    Token name = expectIdent("variable name");
    Node* n = new Node(NK::Let, name.loc);
    n->str = name.text;
    n->flag = mut;
    if (acceptP(":")) n->tyExpr = parseType();
    if (!isP("=")) {
        fail(name.loc, "missing initializer for `" + name.text + "`", "declared here",
             "every variable needs an initial value, e.g. `let " + std::string(mut ? "mut " : "") + name.text +
                 (n->tyExpr ? "" : "") + " = 0`");
    }
    next();
    n->kids.push_back(parseExpr());
    endStmt();
    (void)global;
    return n;
}

Node* Parser::parseIf() {
    Token kw = next();  // if / elif
    if (isKw("let")) {
        next();
        Token name = expectIdent("variable name after `if let`");
        expectP("=", "after `if let " + name.text + "`");
        bool saved = noStruct_;
        noStruct_ = true;
        Node* val = parseExpr();
        noStruct_ = saved;
        Node* n = new Node(NK::IfLet, kw.loc);
        n->str = name.text;
        n->nameLocs.push_back(name.loc);
        Node* then = parseBlock();
        Node* els = nullptr;
        if (isKw("elif")) els = parseIf();
        else if (acceptKw("else")) els = isKw("if") ? parseIf() : parseBlock();
        n->kids = {val, then, els};
        return n;
    }
    bool saved = noStruct_;
    noStruct_ = true;
    Node* cond = parseExpr();
    noStruct_ = saved;
    Node* then = parseBlock();
    Node* els = nullptr;
    if (isKw("elif")) els = parseIf();
    else if (acceptKw("else")) {
        if (isKw("if")) els = parseIf();
        else els = parseBlock();
    }
    Node* n = new Node(NK::If, kw.loc);
    n->kids = {cond, then, els};
    return n;
}

Node* Parser::parseWhile() {
    Token kw = next();
    bool saved = noStruct_;
    noStruct_ = true;
    Node* cond = parseExpr();
    noStruct_ = saved;
    Node* n = new Node(NK::While, kw.loc);
    n->kids = {cond, parseBlock()};
    return n;
}

Node* Parser::parseFor() {
    Token kw = next();
    bool paren = acceptP("(");
    Token a = expectIdent("loop variable after `for`");
    std::vector<Token> vars{a};
    if (acceptP(",")) vars.push_back(expectIdent("second loop variable"));
    if (!isKw("in")) unexpected("`in` after the loop variable", "write `for i in 0..10 { ... }`");
    next();
    bool saved = noStruct_;
    noStruct_ = true;
    Node* start = parseExpr();
    Node* n;
    if (isP("..") || isP("..=")) {
        bool incl = next().text == "..=";
        Node* end = parseExpr();
        if (vars.size() != 1) fail(vars[1].loc, "a range loop has exactly one loop variable");
        n = new Node(NK::ForRange, kw.loc);
        n->str = a.text;
        n->flag = incl;
        n->kids = {start, end};
    } else {
        n = new Node(NK::ForEach, kw.loc);
        n->kids = {start};
        for (auto& v : vars) n->names.push_back(v.text);
    }
    for (auto& v : vars) n->nameLocs.push_back(v.loc);
    noStruct_ = saved;
    if (paren) expectP(")", "to close the `for` header");
    n->kids.push_back(parseBlock());
    return n;
}

// ------------------------------------------------------------ expressions --

Node* Parser::binary(const std::string& op, Node* l, Node* r) {
    Node* n = new Node(NK::Binary, span(l->loc, r->loc));
    n->depth = std::max(l->depth, r->depth) + 1;
    if (n->depth > kMaxChain)
        fail(n->loc, "expression is too long (more than " + std::to_string(kMaxChain) + " operators)", "",
             "split it into several statements");
    n->str = op;
    n->kids = {l, r};
    return n;
}

// A binary operator on a new line continues the expression, except `-`
// which could start a new statement with a negative number.
bool Parser::binOpContinues() const {
    if (nest_ > 0 || !cur().nl) return true;
    return !(isP("-"));
}

Node* Parser::parseExpr() { return parseCoalesce(); }

Node* Parser::parseCoalesce() {
    Node* l = parseOr();
    if (isP("??") && binOpContinues()) {
        next();
        Node* r = parseCoalesce();
        return binary("??", l, r);
    }
    return l;
}

Node* Parser::parseOr() {
    Node* l = parseAnd();
    while ((isP("||") || (cur().kind == Tok::Ident && cur().text == "or")) && binOpContinues()) {
        next();
        l = binary("||", l, parseAnd());
    }
    return l;
}

Node* Parser::parseAnd() {
    Node* l = parseCmp();
    while ((isP("&&") || (cur().kind == Tok::Ident && cur().text == "and")) && binOpContinues()) {
        next();
        l = binary("&&", l, parseCmp());
    }
    return l;
}

static bool isCmpOp(const Token& t) {
    return t.kind == Tok::Punct &&
           (t.text == "==" || t.text == "!=" || t.text == "<" || t.text == ">" || t.text == "<=" || t.text == ">=");
}

Node* Parser::parseCmp() {
    Node* l = parseBitOr();
    if (isCmpOp(cur()) && binOpContinues()) {
        Token op = next();
        Node* r = parseBitOr();
        Node* n = binary(op.text, l, r);
        if (isCmpOp(cur()))
            fail(cur().loc, "comparison operators cannot be chained", "", "split it: `a < b && b < c`");
        return n;
    }
    return l;
}

Node* Parser::parseBitOr() {
    Node* l = parseBitXor();
    while (isP("|") && binOpContinues()) { next(); l = binary("|", l, parseBitXor()); }
    return l;
}
Node* Parser::parseBitXor() {
    Node* l = parseBitAnd();
    while (isP("^") && binOpContinues()) { next(); l = binary("^", l, parseBitAnd()); }
    return l;
}
Node* Parser::parseBitAnd() {
    Node* l = parseShift();
    while (isP("&") && binOpContinues()) { next(); l = binary("&", l, parseShift()); }
    return l;
}
Node* Parser::parseShift() {
    Node* l = parseAdd();
    while ((isP("<<") || isP(">>")) && binOpContinues()) { Token op = next(); l = binary(op.text, l, parseAdd()); }
    return l;
}
Node* Parser::parseAdd() {
    Node* l = parseMul();
    while ((isP("+") || isP("-")) && binOpContinues()) { Token op = next(); l = binary(op.text, l, parseMul()); }
    return l;
}
Node* Parser::parseMul() {
    Node* l = parseCast();
    while ((isP("*") || isP("/") || isP("%")) && binOpContinues()) { Token op = next(); l = binary(op.text, l, parseCast()); }
    return l;
}

Node* Parser::parseCast() {
    Node* e = parseUnary();
    while (isKw("as")) {
        Token kw = next();
        Node* c = new Node(NK::Cast, kw.loc);
        c->kids.push_back(e);
        c->tyExpr = parseType();
        c->loc = span(e->loc, prevEnd());
        e = c;
    }
    return e;
}

Node* Parser::parseUnary() {
    enter(cur().loc);
    Node* r = parseUnaryInner();
    leave();
    return r;
}

Node* Parser::parseUnaryInner() {
    if (isP("-") || isP("!") || isP("~") || (cur().kind == Tok::Ident && cur().text == "not")) {
        Token op = next();
        Node* operand = parseUnary();
        if (op.text == "-" && (operand->kind == NK::IntLit || operand->kind == NK::FloatLit) && !operand->flag) {
            // fold negative literals
            operand->ival = -operand->ival;
            operand->fval = -operand->fval;
            operand->loc = span(op.loc, operand->loc);
            operand->flag = true;  // marks "already negated"
            return operand;
        }
        Node* n = new Node(NK::Unary, span(op.loc, operand->loc));
        n->str = op.text == "not" ? "!" : op.text;
        n->kids.push_back(operand);
        return n;
    }
    return parsePostfix();
}

std::vector<Node*> Parser::parseArgs() {
    std::vector<Node*> args;
    nest_++;
    while (!isP(")")) {
        if (!args.empty()) {
            if (!isP(",")) unexpected("`,` or `)` in argument list");
            next();
            if (isP(")")) break;
        }
        bool saved = noStruct_;
        noStruct_ = false;
        args.push_back(parseExpr());
        noStruct_ = saved;
    }
    nest_--;
    next();  // )
    return args;
}

Node* Parser::parsePostfix() {
    Node* e = parsePrimary();
    while (true) {
        bool sameLine = nest_ > 0 || !cur().nl;
        if (isP("(") && sameLine) {
            Token lp = next();
            Node* c = new Node(NK::Call, e->loc);
            if (e->kind == NK::Ident) {
                c->str = e->str;
                c->kids = parseArgs();
            } else {  // calling the value of an expression, e.g. make_adder(1)(2)
                c->isConst = true;
                c->kids.push_back(e);
                for (Node* a : parseArgs()) c->kids.push_back(a);
            }
            (void)lp;
            c->loc = span(e->loc, prevEnd());
            e = c;
            continue;
        }
        if (isP("[") && sameLine) {
            Token lb = next();
            nest_++;
            bool saved = noStruct_;
            noStruct_ = false;
            Node* a = nullptr;
            Node* b = nullptr;
            bool isSlice = false, incl = false;
            if (!isP("..") && !isP("..=")) a = parseExpr();
            if (isP("..") || isP("..=")) {
                isSlice = true;
                incl = next().text == "..=";
                if (!isP("]")) b = parseExpr();
            }
            noStruct_ = saved;
            nest_--;
            expectP("]", "to close the index");
            Node* n = new Node(isSlice ? NK::Slice : NK::Index, span(e->loc, prevEnd()));
            n->kids = {e, a, b};
            if (!isSlice) n->kids.resize(2);
            n->flag = incl;
            (void)lb;
            e = n;
            continue;
        }
        if (isP("?") && sameLine) {
            Token q = next();
            Node* t = new Node(NK::Try, span(e->loc, q.loc));
            t->kids.push_back(e);
            e = t;
            continue;
        }
        if (isP(".")) {
            next();
            Token name;
            if (cur().kind == Tok::Int) fail(cur().loc, "tuple fields are not supported", "", "");
            name = expectIdent("field or method name after `.`");
            if (isP("(") && (nest_ > 0 || !cur().nl)) {
                next();
                Node* m = new Node(NK::MethodCall, name.loc);
                m->str = name.text;
                m->kids.push_back(e);
                for (Node* a : parseArgs()) m->kids.push_back(a);
                m->loc = span(name.loc, prevEnd());
                e = m;
            } else {
                Node* f = new Node(NK::Field, span(e->loc, name.loc));
                f->str = name.text;
                f->kids.push_back(e);
                f->nameLocs.push_back(name.loc);
                e = f;
            }
            continue;
        }
        break;
    }
    return e;
}

Node* Parser::parsePrimary() {
    const Token& t = cur();
    switch (t.kind) {
    case Tok::Int: {
        Node* n = new Node(NK::IntLit, t.loc);
        n->ival = t.ival;
        n->str = t.text;
        next();
        return n;
    }
    case Tok::Float: {
        Node* n = new Node(NK::FloatLit, t.loc);
        n->fval = t.fval;
        n->str = t.text;
        next();
        return n;
    }
    case Tok::Str: {
        Node* n = new Node(NK::StrLit, t.loc);
        n->str = t.text;
        next();
        return n;
    }
    case Tok::FStr: {
        Token ft = next();
        return parseFString(ft);
    }
    case Tok::Keyword:
        if (t.text == "true" || t.text == "false") {
            Node* n = new Node(NK::BoolLit, t.loc);
            n->flag = t.text == "true";
            next();
            return n;
        }
        if (t.text == "nil") {
            Node* n = new Node(NK::NilLit, t.loc);
            next();
            return n;
        }
        if (t.text == "match") return parseMatch(true);
        if (t.text == "if")
            fail(t.loc, "`if` cannot be used as an expression yet", "", "assign in each branch, or use `match`");
        unexpected("an expression");
    case Tok::Ident: {
        Token name = next();
        if (isP("::")) {
            next();
            Token fn = expectIdent("function or variant name after `::`");
            if (!isP("(") || (cur().nl && nest_ == 0)) {
                Node* p = new Node(NK::Path, span(name.loc, fn.loc));
                p->names.push_back(name.text);
                p->str = fn.text;
                return p;
            }
            next();
            Node* c = new Node(NK::PathCall, span(name.loc, fn.loc));
            c->names.push_back(name.text);
            c->str = fn.text;
            c->kids = parseArgs();
            c->loc = span(name.loc, prevEnd());
            return c;
        }
        if (isP("{") && !noStruct_) return parseStructLit(name);
        Node* n = new Node(NK::Ident, name.loc);
        n->str = name.text;
        return n;
    }
    case Tok::Punct:
        if (t.text == "(") {
            Token lp = next();
            nest_++;
            bool saved = noStruct_;
            noStruct_ = false;
            Node* e = parseExpr();
            noStruct_ = saved;
            nest_--;
            if (isP(",")) fail(cur().loc, "tuples are not supported yet", "", "use a struct or an array");
            expectP(")", "to close the parenthesis");
            (void)lp;
            return e;
        }
        if (t.text == "[") return parseBracketLit();
        if (t.text == "|" || t.text == "||") return parseLambda();
        unexpected("an expression");
    case Tok::Eof:
        unexpected("an expression");
    }
    unexpected("an expression");
}

Node* Parser::parseBracketLit() {
    Token lb = next();  // [
    nest_++;
    bool saved = noStruct_;
    noStruct_ = false;
    Node* result;
    if (isP("]")) {
        result = new Node(NK::ArrayLit, lb.loc);
    } else if (isP(":")) {
        next();
        if (!isP("]")) unexpected("`]` after `[:` (empty map literal)");
        result = new Node(NK::MapLit, lb.loc);
    } else {
        Node* first = parseExpr();
        if (acceptP(":")) {
            result = new Node(NK::MapLit, lb.loc);
            result->kids.push_back(first);
            result->kids.push_back(parseExpr());
            while (acceptP(",")) {
                if (isP("]")) break;
                result->kids.push_back(parseExpr());
                expectP(":", "between map key and value");
                result->kids.push_back(parseExpr());
            }
        } else if (acceptP(";")) {
            result = new Node(NK::ArrayRepeat, lb.loc);
            result->kids.push_back(first);
            result->kids.push_back(parseExpr());
        } else {
            result = new Node(NK::ArrayLit, lb.loc);
            result->kids.push_back(first);
            while (acceptP(",")) {
                if (isP("]")) break;
                result->kids.push_back(parseExpr());
            }
        }
    }
    noStruct_ = saved;
    nest_--;
    expectP("]", "to close the literal");
    result->loc = span(lb.loc, prevEnd());
    return result;
}

Node* Parser::parseStructLit(const Token& name) {
    next();  // {
    nest_++;
    Node* n = new Node(NK::StructLit, name.loc);
    n->str = name.text;
    while (!isP("}")) {
        if (!n->names.empty()) {
            if (!isP(",")) unexpected("`,` or `}` in struct literal");
            next();
            if (isP("}")) break;
        }
        Token f = expectIdent("field name");
        n->names.push_back(f.text);
        n->nameLocs.push_back(f.loc);
        if (acceptP(":")) {
            n->kids.push_back(parseExpr());
        } else {  // shorthand `Point { x, y }`
            Node* id = new Node(NK::Ident, f.loc);
            id->str = f.text;
            n->kids.push_back(id);
        }
    }
    nest_--;
    next();  // }
    n->loc = span(name.loc, prevEnd());
    return n;
}

// match <expr> { <pat> [| <pat>]* [if <guard>] => <body> , ... }
// As a statement an arm body is a block or a statement; as an expression it is
// an expression.
Node* Parser::parseMatch(bool asExpr) {
    Token kw = next();  // match
    Node* m = new Node(NK::Match, kw.loc);
    m->flag = asExpr;
    bool savedNS = noStruct_;
    noStruct_ = true;
    m->kids.push_back(parseExpr());
    noStruct_ = savedNS;
    Token lb = expectP("{", "to start the match arms");
    int savedNest = nest_;
    nest_ = 0;
    while (!isP("}")) {
        if (cur().kind == Tok::Eof) fail(lb.loc, "unclosed `match`: this `{` has no matching `}`");
        if (acceptP(",") || acceptP(";")) continue;
        Node* arm = new Node(NK::MatchArm, cur().loc);
        arm->pats.push_back(parsePattern());
        while (acceptP("|")) arm->pats.push_back(parsePattern());
        Node* guard = nullptr;
        if (isKw("if")) {
            next();
            bool s2 = noStruct_;
            noStruct_ = true;
            guard = parseExpr();
            noStruct_ = s2;
        }
        expectP("=>", "after the match pattern");
        Node* body;
        if (asExpr && !isP("{")) {
            nest_++;
            body = parseExpr();
            nest_--;
        } else if (isP("{")) {  // as a value, a block arm must leave (return/break/continue/panic)
            body = parseBlock();
        } else {
            bool savedArm = armStmt_;
            armStmt_ = true;
            body = parseStmt();
            armStmt_ = savedArm;
        }
        arm->kids = {body, guard};
        m->pats.push_back(arm);
        if (!isP("}") && !acceptP(",") && !cur().nl && !(m->pats.back()->kids[0]->kind == NK::Block))
            unexpected("`,` or a new line between match arms");
    }
    next();  // }
    nest_ = savedNest;
    if (m->pats.empty()) fail(kw.loc, "`match` has no arms");
    return m;
}

// pattern := `_` | literal | name | [Enum::]Variant [`(` pattern, ... `)`] | ok(pattern) | err(pattern)
// Sub-patterns nest to any depth. A bare name is resolved by the checker:
// a unit variant of the matched enum, otherwise a new binding.
Node* Parser::parsePattern() {
    enter(cur().loc);
    Node* p = new Node(NK::Pat, cur().loc);
    if (isIdent() && cur().text == "_") {
        next();
        p->str = "_";
        leave();
        return p;
    }
    if (cur().kind == Tok::Int || cur().kind == Tok::Str || isKw("true") || isKw("false") || isP("-")) {
        p->str = "lit";
        Node* lit = parseUnary();
        if (lit->kind != NK::IntLit && lit->kind != NK::StrLit && lit->kind != NK::BoolLit && lit->kind != NK::FloatLit)
            fail(lit->loc, "patterns can only be literals, `_`, names or enum variants");
        p->kids.push_back(lit);
        p->loc = lit->loc;
        leave();
        return p;
    }
    if (isKw("nil")) fail(cur().loc, "`nil` patterns are not supported", "", "use `if let v = x { } else { }`");
    Token first = expectIdent("a pattern (a literal, `_`, a name or an enum variant)");
    std::string enumName, variant = first.text;
    if (acceptP("::")) {
        enumName = first.text;
        variant = expectIdent("variant name after `::`").text;
    }
    p->names = {enumName, variant};
    p->nameLocs.push_back(first.loc);
    if (acceptP("(")) {
        while (!isP(")")) {
            if (!p->kids.empty()) expectP(",", "between sub-patterns");
            if (isP(")")) break;
            p->kids.push_back(parsePattern());
        }
        expectP(")", "to close the pattern");
        p->flag = true;  // written with a payload list
    }
    p->str = enumName.empty() && !p->flag ? "ident" : "variant";
    if (enumName.empty() && p->flag && (variant == "ok" || variant == "err")) p->str = variant;
    p->loc = span(first.loc, prevEnd());
    leave();
    return p;
}

// |a, b| expr   |a: int| -> int { ... }   || expr
Node* Parser::parseLambda() {
    Token open = next();
    Node* l = new Node(NK::Lambda, open.loc);
    if (open.text == "|") {
        while (!isP("|")) {
            if (!l->names.empty()) expectP(",", "between closure parameters");
            Token pn = expectIdent("closure parameter name");
            l->names.push_back(pn.text);
            l->nameLocs.push_back(pn.loc);
            l->tyExprs.push_back(acceptP(":") ? parseType() : nullptr);
        }
        next();  // closing |
    }
    if (acceptP("->")) {
        l->tyExpr = parseType();
        if (!isP("{")) unexpected("`{` to start the closure body (required after `-> Type`)");
    }
    if (isP("{")) {
        l->kids.push_back(parseBlock());
    } else {
        Node* e = parseExpr();
        Node* ret = new Node(NK::Return, e->loc);
        ret->kids.push_back(e);
        ret->flag = true;  // implicit return of an expression body
        Node* b = new Node(NK::Block, e->loc);
        b->kids.push_back(ret);
        l->kids.push_back(b);
    }
    l->loc = span(open.loc, prevEnd());
    return l;
}

// Decodes escapes in a literal f-string segment (raw text from the file).
static std::string unescapeSegment(const std::string& raw) {
    std::string s;
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c != '\\' || i + 1 >= raw.size()) { s += c; continue; }
        char e = raw[++i];
        switch (e) {
        case 'n': s += '\n'; break;
        case 't': s += '\t'; break;
        case 'r': s += '\r'; break;
        case '0': s += '\0'; break;
        case 'e': s += '\x1b'; break;
        case 'x':
            if (i + 2 < raw.size() + 1) {
                s += (char)std::strtol(raw.substr(i + 1, 2).c_str(), nullptr, 16);
                i += 2;
            }
            break;
        default: s += e; break;  // \\ \" \{ \}
        }
    }
    return s;
}

Node* Parser::parseFString(const Token& t) {
    Node* n = new Node(NK::FStr, t.loc);
    const std::string& raw = t.text;
    std::string seg;
    size_t i = 0;
    auto bodyLoc = [&](size_t idx, size_t len) { return Lexer::locAt(file_, t.bodyOff + (uint32_t)idx, (uint32_t)len); };
    while (i < raw.size()) {
        char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) { seg += raw.substr(i, 2); i += 2; continue; }
        if (c == '{' && i + 1 < raw.size() && raw[i + 1] == '{') { seg += "{"; i += 2; continue; }
        if (c == '}' && i + 1 < raw.size() && raw[i + 1] == '}') { seg += "}"; i += 2; continue; }
        if (c == '}') fail(bodyLoc(i, 1), "unmatched `}` in f-string", "", "write `}}` for a literal brace");
        if (c != '{') { seg += c; i++; continue; }
        // interpolation: find matching '}' and an optional top-level ':' spec
        size_t start = i + 1, j = start;
        int depth = 0;
        size_t colon = std::string::npos;
        while (j < raw.size()) {
            char d = raw[j];
            if (d == '"') {
                j++;
                while (j < raw.size() && raw[j] != '"') { if (raw[j] == '\\') j++; j++; }
                j++;
                continue;
            }
            if (d == '(' || d == '[' || d == '{') depth++;
            else if (d == ')' || d == ']') depth--;
            else if (d == '}') { if (depth == 0) break; depth--; }
            else if (d == ':' && depth == 0 && colon == std::string::npos) {
                bool dbl = (j + 1 < raw.size() && raw[j + 1] == ':') || (j > start && raw[j - 1] == ':');
                if (!dbl) colon = j;
            }
            j++;
        }
        if (j >= raw.size()) fail(bodyLoc(i, 1), "unclosed `{` in f-string", "", "write `{{` for a literal brace");
        size_t exprEnd = colon == std::string::npos ? j : colon;
        std::string spec = colon == std::string::npos ? "" : raw.substr(colon + 1, j - colon - 1);
        {
            bool blank = true;
            for (size_t k = start; k < exprEnd; k++) if (!std::isspace((unsigned char)raw[k])) blank = false;
            if (blank) fail(bodyLoc(i, j - i + 1), "empty expression in f-string", "", "write `{{}}` for literal braces");
        }
        Lexer sub(file_, diag_, t.bodyOff + (uint32_t)start, t.bodyOff + (uint32_t)exprEnd);
        std::vector<Token> toks = sub.tokenize();
        Parser p(file_, diag_, prog_, toks);
        p.nest_ = 1;
        Node* e = p.parseExpr();
        if (p.cur().kind != Tok::Eof)
            p.fail(p.cur().loc, "unexpected " + p.describe(p.cur()) + " in f-string expression");
        n->names.push_back(unescapeSegment(seg));
        seg.clear();
        n->kids.push_back(e);
        n->specs.push_back(spec);
        n->nameLocs.push_back(bodyLoc(colon == std::string::npos ? i : colon, colon == std::string::npos ? 1 : spec.size() + 1));
        i = j + 1;
    }
    n->names.push_back(unescapeSegment(seg));
    return n;
}

}  // namespace arc
