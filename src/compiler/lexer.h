#pragma once

#include "ast.h"

namespace arc {

enum class Tok { Eof, Ident, Keyword, Int, Float, Str, FStr, Punct };

struct Token {
    Tok kind = Tok::Eof;
    std::string text;      // identifier / keyword / punct / decoded string / raw f-string body
    int64_t ival = 0;
    double fval = 0;
    Loc loc;
    bool nl = false;       // a newline precedes this token
    uint32_t bodyOff = 0;  // f-string: file offset of the first body byte
};

class Lexer {
public:
    Lexer(SourceFile* f, Diagnostics& d, uint32_t start = 0, uint32_t end = UINT32_MAX);
    std::vector<Token> tokenize();
    static Loc locAt(const SourceFile* f, uint32_t off, uint32_t len = 1);

private:
    SourceFile* file_;
    Diagnostics& diag_;
    uint32_t pos_, end_;

    char peek(uint32_t k = 0) const { return pos_ + k < end_ ? file_->text[pos_ + k] : '\0'; }
    [[noreturn]] void fail(uint32_t off, uint32_t len, const std::string& msg, const std::string& help = "");
    Token lexNumber();
    Token lexString(bool fstring);
};

bool isKeyword(const std::string& s);

}  // namespace arc
