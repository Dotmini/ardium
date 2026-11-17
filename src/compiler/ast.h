// Arc compiler — core data structures: sources, diagnostics, types, AST.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace arc {

// ---------------------------------------------------------------- sources --

struct SourceFile {
    int id = 0;
    std::string path;     // path as given / displayed
    std::string text;
    std::vector<uint32_t> lineStarts;
    void computeLines();
    std::string lineText(uint32_t line) const;  // 1-based, without newline
};

struct Loc {
    int file = -1;
    uint32_t off = 0;
    uint32_t line = 0, col = 0;  // 1-based
    uint32_t len = 1;            // span length in bytes (for carets)
};

// ------------------------------------------------------------ diagnostics --

struct Diag {
    enum Level { Error, Warning } level = Error;
    std::string msg;
    Loc loc;
    std::string label;               // text next to the caret
    std::vector<std::string> notes;  // "help: ..." / "note: ..."
};

class Diagnostics {
public:
    std::vector<SourceFile*>* files = nullptr;
    std::vector<Diag> list;
    int errorCount = 0;

    Diag& error(const Loc& loc, const std::string& msg, const std::string& label = "");
    Diag& warning(const Loc& loc, const std::string& msg, const std::string& label = "");
    void render(std::ostream& os, bool color) const;
    bool hasErrors() const { return errorCount > 0; }
};

// Thrown to abort a compilation phase after an unrecoverable error.
struct CompileAbort {};

// ------------------------------------------------------------------ types --

enum class TK {
    Error, Void, Int, Float, Bool, Str, Nil, Array, Map, Struct, Optional,
    Enum,    // sd->isEnum; heap object with a tag
    Result,  // `!T`: a T or an error (elem = T, may be Void)
    Func,    // `fn(A, B) -> R`: params + elem = return type
    ErrVal,  // type of an `err(...)` expression before it meets a `!T`
};

struct StructDecl;

struct Type {
    TK kind;
    Type* elem = nullptr;  // Array elem, Map value, Optional payload
    Type* key = nullptr;   // Map key
    StructDecl* sd = nullptr;
    std::vector<Type*> params;  // Func parameter types
    int id = 0;

    // the C value is a single (nullable) pointer to a GC object
    bool isHeap() const {
        switch (kind) {
        case TK::Str: case TK::Array: case TK::Map: case TK::Struct: case TK::Enum: case TK::Func: return true;
        case TK::Optional: return elem->isHeap();
        case TK::Result: return elem->kind == TK::Void || elem->isHeap();
        default: return false;
        }
    }
    // the C value contains a GC pointer (possibly inside a small struct)
    bool hasPtr() const { return isHeap() || kind == TK::Result; }
    bool isNumeric() const { return kind == TK::Int || kind == TK::Float; }
    bool isError() const { return kind == TK::Error; }
    std::string str() const;
};

class Types {
public:
    static Types& get();
    Type* errorT; Type* voidT; Type* intT; Type* floatT; Type* boolT; Type* strT; Type* nilT;
    Type* array(Type* elem);
    Type* map(Type* key, Type* val);
    Type* optional(Type* inner);
    Type* structT(StructDecl* sd);
    Type* enumT(StructDecl* sd);
    Type* result(Type* inner);
    Type* func(const std::vector<Type*>& params, Type* ret);
    Type* errValT;
    std::vector<Type*> all;
private:
    Types();
    Type* make(TK k);
    std::map<Type*, Type*> arrays_, optionals_, results_;
    std::map<std::pair<std::vector<Type*>, Type*>, Type*> funcs_;
    std::map<std::pair<Type*, Type*>, Type*> maps_;
};

// ------------------------------------------------------------------- AST --

struct TypeExpr {
    enum Kind { Name, Array, Map, Optional, Result, Func } kind = Name;
    std::string name;
    TypeExpr* a = nullptr;
    TypeExpr* b = nullptr;             // Map value / Func return
    std::vector<TypeExpr*> params;     // Func parameters
    Loc loc;
};

enum class NK {
    // expressions
    IntLit, FloatLit, BoolLit, StrLit, FStr, NilLit, Ident,
    Unary, Binary, Call, MethodCall, PathCall, Field, Index, Slice,
    ArrayLit, ArrayRepeat, MapLit, StructLit, Cast, Wrap,
    Match,     // kids[0] = scrutinee, pats = arms (MatchArm); flag = used as an expression
    MatchArm,  // pats = alternative patterns, kids[0] = body, kids[1] = guard or null
    Pat,       // str = "_", "lit", "ident", "variant", "ok", "err"; see parser
    Lambda,    // names = params, specs unused, kids[0] = body block; fn = synthesized FnDecl
    Try,       // kids[0] `?`
    Path,      // names[0] = type, str = variant (unit enum variant)
    // statements
    Let, IfLet, Assign, ExprStmt, If, While, Loop, ForRange, ForEach,
    Break, Continue, Return, Block,
};

struct Var;
struct FnDecl;

// Builtin ids resolved by the checker (free functions and methods).
enum class BI {
    None,
    // free functions
    Print, Println, Eprint, Eprintln, Str, Int, Float, Len, ParseInt, ParseFloat,
    Abs, Min, Max, Sqrt, Pow, Floor, Ceil, Round, Sin, Cos, Tan, Atan2, Exp, Log, Log10, IPow,
    Assert, AssertEq, Panic, Exit, ReadLine, ReadFile, WriteFile, AppendFile, FileExists, Args,
    TimeNs, TimeMs, UnixTime, SleepMs, RandInt, RandFloat, Seed, Chr, Ord, Getenv, RunCmd, GcCount,
    // str methods
    SLen, SIsEmpty, SContains, SStartsWith, SEndsWith, SFind, SUpper, SLower, STrim, SSplit, SSplitWs,
    SLines, SReplace, SRepeat, SSlice, SByteAt, SChars, SCharCount, SToInt, SToFloat,
    // array methods
    ALen, AIsEmpty, APush, APop, AInsert, ARemove, AClear, AContains, AIndexOf, AReverse, ASort,
    AJoin, ASlice, ACopy, ALast, AFirst,
    // map methods
    MLen, MIsEmpty, MContains, MGet, MGetOr, MRemove, MKeys, MValues, MClear,
    // optional methods
    OUnwrap, OIsSome, OIsNil, OUnwrapOr,
    // results
    Err, Ok, RIsOk, RIsErr, RUnwrap, RUnwrapOr, RError,
    // higher-order array methods
    AMap, AFilter, AAny, AAll, AFind, AFold, ASortBy, ASum, AMin, AMax,
    // more stdlib
    Exec, ListDir, MakeDir, RemoveFile,
    // calling a value of function type (n->var or kids[0] holds the closure)
    CallValue,
};

struct Node {
    NK kind;
    Loc loc;
    std::string str;                 // identifier / operator / field / method / literal text
    int64_t ival = 0;
    double fval = 0;
    bool flag = false;               // mut / inclusive range / bool literal value
    std::vector<Node*> kids;
    std::vector<std::string> names;  // struct-literal fields, f-string segments, loop vars
    std::vector<std::string> specs;  // f-string format specs
    std::vector<Loc> nameLocs;
    std::vector<Node*> pats;         // match arms / arm patterns
    std::vector<TypeExpr*> tyExprs;  // lambda parameter types (null when inferred)
    TypeExpr* tyExpr = nullptr;
    // semantic annotations
    Type* type = nullptr;
    Var* var = nullptr;
    Var* var2 = nullptr;
    std::vector<Var*> vars;          // pattern bindings
    FnDecl* fn = nullptr;
    BI builtin = BI::None;
    bool isConst = false;
    uint32_t depth = 0;              // height of a binary-operator chain (parser limit)

    Node(NK k, Loc l) : kind(k), loc(l) {}
};

struct Var {
    std::string name;
    Type* type = nullptr;
    bool mut = false;
    bool isGlobal = false;
    bool isParam = false;
    bool used = false;
    bool assigned = false;
    bool isCapture = false;  // a closure's copy of an outer variable
    Var* captureOf = nullptr;
    Loc loc;
    std::string cname;
};

struct Param {
    std::string name;
    TypeExpr* te = nullptr;
    Loc loc;
    bool mut = false;
    Var* var = nullptr;
};

struct FnDecl {
    std::string name;
    Loc loc;
    std::vector<Param> params;
    TypeExpr* retTe = nullptr;
    Node* body = nullptr;
    StructDecl* owner = nullptr;
    bool hasSelf = false;
    bool selfMut = false;
    bool isTest = false;
    bool isLambda = false;
    int lambdaId = 0;
    std::vector<std::pair<Var*, Var*>> captures;  // (outer variable, closure copy)
    std::vector<std::string> attrs;
    // semantic
    Type* ret = nullptr;
    std::vector<Type*> paramTypes;
    int state = 0;            // 0 = unchecked, 1 = checking, 2 = done
    bool retFromBody = false; // return type inferred from body
    Var* selfVar = nullptr;
    std::string cname;
    std::vector<Var*> locals;
    std::string displayName() const;
};

struct FieldDecl {
    std::string name;
    TypeExpr* te = nullptr;
    Type* type = nullptr;
    Loc loc;
};

struct Variant {
    std::string name;
    Loc loc;
    std::vector<TypeExpr*> tes;
    std::vector<Type*> types;
    int firstField = 0;  // payload i lives in fields[firstField + i]
};

struct StructDecl {
    std::string name;
    Loc loc;
    bool isEnum = false;
    std::vector<Variant> variants;  // enums only; payloads are flattened into `fields`
    int variantIndex(const std::string& n) const {
        for (size_t i = 0; i < variants.size(); i++) if (variants[i].name == n) return (int)i;
        return -1;
    }
    std::vector<FieldDecl> fields;
    std::map<std::string, FnDecl*> methods;
    Type* type = nullptr;
    int fieldIndex(const std::string& n) const {
        for (size_t i = 0; i < fields.size(); i++) if (fields[i].name == n) return (int)i;
        return -1;
    }
};

struct Program {
    std::vector<SourceFile*> files;
    std::vector<FnDecl*> fns;          // free functions (incl. tests)
    std::vector<StructDecl*> structs;
    std::vector<FnDecl*> methods;      // all impl functions
    std::vector<FnDecl*> lambdas;      // closures, synthesized by the checker
    std::vector<Node*> globals;        // top-level Let nodes (source order)
    std::vector<Node*> globalInitOrder; // dependency order, filled by the checker
    std::map<std::string, FnDecl*> fnByName;
    std::map<std::string, StructDecl*> structByName;
    FnDecl* mainFn = nullptr;
};

}  // namespace arc
