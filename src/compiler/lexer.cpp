#include "lexer.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace arc {

static const std::set<std::string> kKeywords = {
    "fn", "let", "mut", "var", "if", "elif", "else", "while", "for", "in", "loop",
    "break", "continue", "return", "struct", "impl", "true", "false", "nil", "import", "as",
};

bool isKeyword(const std::string& s) { return kKeywords.count(s) > 0; }

Lexer::Lexer(SourceFile* f, Diagnostics& d, uint32_t start, uint32_t end)
    : file_(f), diag_(d), pos_(start), end_(std::min<uint32_t>(end, (uint32_t)f->text.size())) {}

Loc Lexer::locAt(const SourceFile* f, uint32_t off, uint32_t len) {
    Loc l;
    l.file = f->id;
    l.off = off;
    l.len = len;
    auto it = std::upper_bound(f->lineStarts.begin(), f->lineStarts.end(), off);
    size_t lineIdx = (size_t)(it - f->lineStarts.begin());  // 1-based line
    l.line = (uint32_t)lineIdx;
    l.col = off - f->lineStarts[lineIdx - 1] + 1;
    return l;
}

void Lexer::fail(uint32_t off, uint32_t len, const std::string& msg, const std::string& help) {
    Diag& d = diag_.error(locAt(file_, off, len), msg);
    if (!help.empty()) d.notes.push_back("help: " + help);
    throw CompileAbort{};
}

static bool isIdentStart(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 0x80; }
static bool isIdentChar(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

std::vector<Token> Lexer::tokenize() {
    static const char* puncts[] = {
        "..=", "...", "<<=", ">>=", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "%=",
        "&=", "|=", "^=", "<<", ">>", "->", "=>", "::", "..", "??",
        "+", "-", "*", "/", "%", "=", "<", ">", "!", "&", "|", "^", "~", "(", ")", "{", "}", "[", "]",
        ",", ";", ":", ".", "@", "?", "#",
    };
    std::vector<Token> out;
    bool nl = true;
    while (true) {
        // skip whitespace and comments
        while (pos_ < end_) {
            char c = peek();
            if (c == '\n') { nl = true; pos_++; }
            else if (c == ' ' || c == '\t' || c == '\r') pos_++;
            else if (c == '/' && peek(1) == '/') { while (pos_ < end_ && peek() != '\n') pos_++; }
            else if (c == '/' && peek(1) == '*') {
                uint32_t start = pos_;
                int depth = 0;
                while (pos_ < end_) {
                    if (peek() == '/' && peek(1) == '*') { depth++; pos_ += 2; }
                    else if (peek() == '*' && peek(1) == '/') { depth--; pos_ += 2; if (depth == 0) break; }
                    else { if (peek() == '\n') nl = true; pos_++; }
                }
                if (depth != 0) fail(start, 2, "unterminated block comment");
            } else if (c == '#' && peek(1) == '!' && pos_ == 0) {
                while (pos_ < end_ && peek() != '\n') pos_++;  // shebang
            } else break;
        }
        if (pos_ >= end_) {
            Token t;
            t.kind = Tok::Eof;
            t.loc = locAt(file_, std::min<uint32_t>(pos_, file_->text.size()), 1);
            t.nl = true;
            out.push_back(t);
            break;
        }
        uint32_t start = pos_;
        unsigned char c = (unsigned char)peek();
        Token t;
        if (c == 'f' && peek(1) == '"') {
            pos_++;
            t = lexString(true);
            t.loc = locAt(file_, start, pos_ - start);
        } else if (isIdentStart(c)) {
            while (pos_ < end_ && isIdentChar((unsigned char)peek())) pos_++;
            t.text = file_->text.substr(start, pos_ - start);
            t.kind = isKeyword(t.text) ? Tok::Keyword : Tok::Ident;
            t.loc = locAt(file_, start, pos_ - start);
        } else if (std::isdigit(c)) {
            t = lexNumber();
            t.loc = locAt(file_, start, pos_ - start);
        } else if (c == '"') {
            t = lexString(false);
            t.loc = locAt(file_, start, pos_ - start);
        } else if (c == '\'') {
            fail(start, 1, "single quotes are not string delimiters", "use double quotes: \"...\"");
        } else {
            bool found = false;
            for (const char* p : puncts) {
                size_t n = std::char_traits<char>::length(p);
                if (file_->text.compare(pos_, n, p) == 0 && pos_ + n <= end_) {
                    t.kind = Tok::Punct;
                    t.text = p;
                    pos_ += (uint32_t)n;
                    found = true;
                    break;
                }
            }
            if (!found) fail(start, 1, std::string("unexpected character '") + (char)c + "'");
            t.loc = locAt(file_, start, pos_ - start);
        }
        t.nl = nl;
        nl = false;
        out.push_back(t);
    }
    return out;
}

Token Lexer::lexNumber() {
    uint32_t start = pos_;
    Token t;
    std::string digits;
    int base = 10;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) { base = 16; pos_ += 2; }
    else if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) { base = 2; pos_ += 2; }
    else if (peek() == '0' && (peek(1) == 'o' || peek(1) == 'O')) { base = 8; pos_ += 2; }
    bool isFloat = false;
    while (pos_ < end_) {
        char c = peek();
        if (c == '_') { pos_++; continue; }
        if (base == 16 ? std::isxdigit((unsigned char)c) : std::isdigit((unsigned char)c)) { digits += c; pos_++; continue; }
        if (base == 10 && c == '.' && std::isdigit((unsigned char)peek(1)) && !isFloat) {
            isFloat = true; digits += c; pos_++; continue;
        }
        if (base == 10 && (c == 'e' || c == 'E') &&
            (std::isdigit((unsigned char)peek(1)) || ((peek(1) == '+' || peek(1) == '-') && std::isdigit((unsigned char)peek(2))))) {
            isFloat = true;
            digits += c; pos_++;
            if (peek() == '+' || peek() == '-') { digits += peek(); pos_++; }
            continue;
        }
        break;
    }
    if (pos_ < end_ && isIdentChar((unsigned char)peek()))
        fail(start, pos_ - start + 1, "invalid number literal");
    if (digits.empty()) fail(start, pos_ - start, "invalid number literal");
    if (isFloat) {
        t.kind = Tok::Float;
        t.fval = std::strtod(digits.c_str(), nullptr);
        t.text = digits;
        return t;
    }
    t.kind = Tok::Int;
    uint64_t v = 0;
    for (char c : digits) {
        int d = std::isdigit((unsigned char)c) ? c - '0' : (std::tolower(c) - 'a' + 10);
        if (d >= base) fail(start, pos_ - start, "invalid digit '" + std::string(1, c) + "' in base-" + std::to_string(base) + " literal");
        if (v > (UINT64_MAX - (uint64_t)d) / (uint64_t)base)
            fail(start, pos_ - start, "integer literal is too large", "the maximum int value is 9223372036854775807");
        v = v * (uint64_t)base + (uint64_t)d;
    }
    if (v > (uint64_t)INT64_MAX && base == 10)
        fail(start, pos_ - start, "integer literal is too large", "the maximum int value is 9223372036854775807");
    t.ival = (int64_t)v;
    t.text = file_->text.substr(start, pos_ - start);
    return t;
}

static void appendUtf8(std::string& s, uint32_t cp) {
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
    else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
}

Token Lexer::lexString(bool fstring) {
    uint32_t quote = pos_;
    pos_++;  // opening quote
    Token t;
    t.kind = fstring ? Tok::FStr : Tok::Str;
    t.bodyOff = pos_;
    std::string s;
    int braceDepth = 0;
    while (true) {
        if (pos_ >= end_) fail(quote, 1, "unterminated string literal");
        char c = peek();
        if (fstring) {
            // keep the raw body; nested strings inside {...} are skipped over
            if (c == '{' && peek(1) != '{') braceDepth++;
            else if (c == '{') { s += "{{"; pos_ += 2; continue; }
            else if (c == '}' && braceDepth > 0) braceDepth--;
            if (c == '"' && braceDepth > 0) {
                // a string literal inside an interpolation
                s += c; pos_++;
                while (pos_ < end_ && peek() != '"') {
                    if (peek() == '\\') { s += peek(); pos_++; }
                    if (pos_ < end_) { s += peek(); pos_++; }
                }
                if (pos_ >= end_) fail(quote, 1, "unterminated string literal");
                s += peek(); pos_++;
                continue;
            }
            if (c == '"') { pos_++; break; }
            if (c == '\\' && braceDepth == 0) { s += c; pos_++; if (pos_ < end_) { s += peek(); pos_++; } continue; }
            s += c; pos_++;
            continue;
        }
        if (c == '"') { pos_++; break; }
        if (c == '\\') {
            uint32_t escStart = pos_;
            pos_++;
            char e = peek();
            pos_++;
            switch (e) {
            case 'n': s += '\n'; break;
            case 't': s += '\t'; break;
            case 'r': s += '\r'; break;
            case '0': s += '\0'; break;
            case '\\': s += '\\'; break;
            case '"': s += '"'; break;
            case '\'': s += '\''; break;
            case '{': s += '{'; break;
            case '}': s += '}'; break;
            case 'e': s += '\x1b'; break;
            case 'x': {
                std::string hex;
                for (int i = 0; i < 2 && std::isxdigit((unsigned char)peek()); i++) hex += file_->text[pos_++];
                if (hex.size() != 2) fail(escStart, pos_ - escStart, "invalid \\x escape", "use two hex digits, e.g. \\x41");
                s += (char)std::strtol(hex.c_str(), nullptr, 16);
                break;
            }
            case 'u': {
                if (peek() != '{') fail(escStart, 2, "invalid unicode escape", "use \\u{XXXX}");
                pos_++;
                std::string hex;
                while (std::isxdigit((unsigned char)peek()) && hex.size() < 6) hex += file_->text[pos_++];
                if (peek() != '}' || hex.empty()) fail(escStart, pos_ - escStart, "invalid unicode escape", "use \\u{XXXX}");
                pos_++;
                appendUtf8(s, (uint32_t)std::strtoul(hex.c_str(), nullptr, 16));
                break;
            }
            default:
                fail(escStart, 2, std::string("unknown escape sequence '\\") + e + "'");
            }
            continue;
        }
        s += c;
        pos_++;
    }
    t.text = s;
    return t;
}

}  // namespace arc
