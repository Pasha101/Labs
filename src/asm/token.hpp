#pragma once

#include <string>

struct Token {
    enum class Kind {
        Ident,
        Number,
        Comma,
        Colon,
        LBracket,
        RBracket,
        Newline,
        Eof
    };

    Kind kind;
    std::string text;
    int line;
    Token* next;
};

void destroy(Token* head);
const char* token_kind_name(Token::Kind kind);