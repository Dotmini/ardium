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

enum class TK { Error, Void, Int, Float, Bool, Str, Nil, Array, Map, Struct, Optional };

struct StructDecl;

struct Type {
    TK kind;
    Type* elem = nullptr;  // Array elem, Map value, Optional payload
    Type* key = nullptr;   // Map key
    StructDecl* sd = nullptr;
    int id = 0;

    bool isHeap() const {
        switch (kind) {
        case TK::Str: case TK::Array: case TK::Map: case TK::Struct: return true;
        case TK::Optional: return elem->isHeap();
        default: return false;
        }
    }
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
    std::vector<Type*> all;
private:
    Types();
    Type* make(TK k);
    std::map<Type*, Type*> arrays_, optionals_;
    std::map<std::pair<Type*, Type*>, Type*> maps_;
};

// ------------------------------------------------------------------- AST --

struct TypeExpr {
    enum Kind { Name, Array, Map, Optional } kind = Name;
    std::string name;
    TypeExpr* a = nullptr;
    TypeExpr* b = nullptr;
    Loc loc;
};

enum class NK {
    // expressions
    IntLit, FloatLit, BoolLit, StrLit, FStr, NilLit, Ident,
    Unary, Binary, Call, MethodCall, PathCall, Field, Index, Slice,
    ArrayLit, ArrayRepeat, MapLit, StructLit, Cast, Wrap,
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
    TypeExpr* tyExpr = nullptr;
    // semantic annotations
    Type* type = nullptr;
    Var* var = nullptr;
    Var* var2 = nullptr;
    FnDecl* fn = nullptr;
    BI builtin = BI::None;
    bool isConst = false;

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

struct StructDecl {
    std::string name;
    Loc loc;
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
    std::vector<Node*> globals;        // top-level Let nodes (source order)
    std::vector<Node*> globalInitOrder; // dependency order, filled by the checker
    std::map<std::string, FnDecl*> fnByName;
    std::map<std::string, StructDecl*> structByName;
    FnDecl* mainFn = nullptr;
};

}  // namespace arc
