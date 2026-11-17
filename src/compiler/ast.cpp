#include "ast.h"

#include <algorithm>
#include <sstream>

namespace arc {

// ---------------------------------------------------------------- sources --

void SourceFile::computeLines() {
    lineStarts.clear();
    lineStarts.push_back(0);
    for (uint32_t i = 0; i < text.size(); i++)
        if (text[i] == '\n') lineStarts.push_back(i + 1);
}

std::string SourceFile::lineText(uint32_t line) const {
    if (line == 0 || line > lineStarts.size()) return "";
    uint32_t s = lineStarts[line - 1];
    uint32_t e = s;
    while (e < text.size() && text[e] != '\n') e++;
    std::string r = text.substr(s, e - s);
    if (!r.empty() && r.back() == '\r') r.pop_back();
    return r;
}

// ------------------------------------------------------------ diagnostics --

Diag& Diagnostics::error(const Loc& loc, const std::string& msg, const std::string& label) {
    errorCount++;
    list.push_back(Diag{Diag::Error, msg, loc, label, {}});
    return list.back();
}

Diag& Diagnostics::warning(const Loc& loc, const std::string& msg, const std::string& label) {
    list.push_back(Diag{Diag::Warning, msg, loc, label, {}});
    return list.back();
}

static std::string expandTabs(const std::string& s, size_t upto, size_t* outCol) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (i == upto && outCol) *outCol = r.size();
        if (s[i] == '\t') r += "    ";
        else r += s[i];
    }
    if (upto >= s.size() && outCol) *outCol = r.size() + (upto - s.size());
    return r;
}

// display width of a UTF-8 byte range (counts code points)
static size_t displayWidth(const std::string& s, size_t from, size_t to) {
    size_t w = 0;
    for (size_t i = from; i < to && i < s.size(); i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) w++;
    return w;
}

void Diagnostics::render(std::ostream& os, bool color) const {
    const char* red = color ? "\x1b[1;31m" : "";
    const char* yellow = color ? "\x1b[1;33m" : "";
    const char* blue = color ? "\x1b[1;34m" : "";
    const char* bold = color ? "\x1b[1m" : "";
    const char* reset = color ? "\x1b[0m" : "";
    const char* cyan = color ? "\x1b[1;36m" : "";

    for (const Diag& d : list) {
        const char* lc = d.level == Diag::Error ? red : yellow;
        os << lc << (d.level == Diag::Error ? "error" : "warning") << reset << bold << ": " << d.msg << reset << "\n";
        SourceFile* sf = nullptr;
        if (files && d.loc.file >= 0 && d.loc.file < (int)files->size()) sf = (*files)[d.loc.file];
        if (sf && d.loc.line > 0) {
            std::string lineNo = std::to_string(d.loc.line);
            std::string pad(lineNo.size(), ' ');
            os << pad << blue << "--> " << reset << sf->path << ":" << d.loc.line << ":" << d.loc.col << "\n";
            os << pad << blue << " |" << reset << "\n";
            std::string raw = sf->lineText(d.loc.line);
            size_t byteCol = d.loc.col > 0 ? d.loc.col - 1 : 0;
            // expand tabs and convert byte offsets to display columns
            std::string shown;
            size_t caretStart = 0, caretLen = 0;
            {
                size_t dispCol = 0;
                std::string prefix = raw.substr(0, std::min(byteCol, raw.size()));
                size_t tmp;
                std::string ex = expandTabs(prefix, prefix.size(), &tmp);
                dispCol = displayWidth(ex, 0, ex.size());
                shown = expandTabs(raw, raw.size(), &tmp);
                caretStart = dispCol;
                size_t endByte = std::min<size_t>(raw.size(), byteCol + std::max<uint32_t>(1, d.loc.len));
                caretLen = std::max<size_t>(1, displayWidth(raw, byteCol, endByte));
                if (byteCol >= raw.size()) caretLen = 1;
            }
            os << blue << lineNo << " |" << reset << " " << shown << "\n";
            os << pad << blue << " |" << reset << " " << std::string(caretStart, ' ') << lc << std::string(caretLen, '^');
            if (!d.label.empty()) os << " " << d.label;
            os << reset << "\n";
            if (!d.notes.empty()) os << pad << blue << " |" << reset << "\n";
            for (const std::string& n : d.notes) os << pad << blue << " = " << reset << cyan << n << reset << "\n";
        } else {
            for (const std::string& n : d.notes) os << "  = " << n << "\n";
        }
        os << "\n";
    }
}

// ------------------------------------------------------------------ types --

std::string Type::str() const {
    switch (kind) {
    case TK::Error: return "{error}";
    case TK::Void: return "void";
    case TK::Int: return "int";
    case TK::Float: return "float";
    case TK::Bool: return "bool";
    case TK::Str: return "str";
    case TK::Nil: return "nil";
    case TK::Array: return "[" + elem->str() + "]";
    case TK::Map: return "[" + key->str() + ": " + elem->str() + "]";
    case TK::Struct: return sd ? sd->name : "struct";
    case TK::Optional: return "?" + elem->str();
    }
    return "?";
}

Types& Types::get() {
    static Types t;
    return t;
}

Type* Types::make(TK k) {
    Type* t = new Type{k};
    t->id = (int)all.size();
    all.push_back(t);
    return t;
}

Types::Types() {
    errorT = make(TK::Error);
    voidT = make(TK::Void);
    intT = make(TK::Int);
    floatT = make(TK::Float);
    boolT = make(TK::Bool);
    strT = make(TK::Str);
    nilT = make(TK::Nil);
}

Type* Types::array(Type* elem) {
    auto it = arrays_.find(elem);
    if (it != arrays_.end()) return it->second;
    Type* t = make(TK::Array);
    t->elem = elem;
    arrays_[elem] = t;
    return t;
}

Type* Types::map(Type* key, Type* val) {
    auto k = std::make_pair(key, val);
    auto it = maps_.find(k);
    if (it != maps_.end()) return it->second;
    Type* t = make(TK::Map);
    t->key = key;
    t->elem = val;
    maps_[k] = t;
    return t;
}

Type* Types::optional(Type* inner) {
    auto it = optionals_.find(inner);
    if (it != optionals_.end()) return it->second;
    Type* t = make(TK::Optional);
    t->elem = inner;
    optionals_[inner] = t;
    return t;
}

Type* Types::structT(StructDecl* sd) {
    Type* t = make(TK::Struct);
    t->sd = sd;
    return t;
}

std::string FnDecl::displayName() const {
    if (owner) return owner->name + "::" + name;
    return name;
}

}  // namespace arc
