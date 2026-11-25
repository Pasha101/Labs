#include "lexer.hpp"

#include <cctype>

struct Lexer {
    const std::string& src;
    std::size_t i = 0;
    int line = 1;
    Token* head = nullptr;
    Token* tail = nullptr;
    std::string error;
};

static void emit(Lexer& lx, Token::Kind kind, const std::string& text) {
    Token* t = new Token{kind, text, lx.line, nullptr};
    if (!lx.head) {
        lx.head = lx.tail = t;
    } else {
        lx.tail->next = t;
        lx.tail = t;
    }
}

static bool fail(Lexer& lx, const std::string& msg) {
    lx.error = "line " + std::to_string(lx.line) + ": " + msg;
    return false;
}

void destroy(Token* head) {
    while (head) {
        Token* next = head->next;
        delete head;
        head = next;
    }
}

const char* token_kind_name(Token::Kind kind) {
    switch (kind) {
        case Token::Kind::Ident:    return "Ident";
        case Token::Kind::Number:   return "Number";
        case Token::Kind::Comma:    return "Comma";
        case Token::Kind::Colon:    return "Colon";
        case Token::Kind::LBracket: return "LBracket";
        case Token::Kind::RBracket: return "RBracket";
        case Token::Kind::Newline:  return "Newline";
        case Token::Kind::Eof:      return "Eof";
    }
    return "Unknown";
}

static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static bool number(Lexer& lx) {
    std::size_t start = lx.i;

    if (lx.src[lx.i] == '0' && lx.i + 1 < lx.src.size() &&
        (lx.src[lx.i + 1] == 'x' || lx.src[lx.i + 1] == 'X')) {
        lx.i += 2;
        std::size_t digits_start = lx.i;
        while (lx.i < lx.src.size() && std::isxdigit(static_cast<unsigned char>(lx.src[lx.i]))) {
            ++lx.i;
        }
        if (lx.i == digits_start || (lx.i < lx.src.size() && is_ident_char(lx.src[lx.i]))) {
            return fail(lx, "bad hex number");
        }
        emit(lx, Token::Kind::Number, lx.src.substr(start, lx.i - start));
        return true;
    }

    if (lx.src[lx.i] == '0' && lx.i + 1 < lx.src.size() &&
        (lx.src[lx.i + 1] == 'b' || lx.src[lx.i + 1] == 'B')) {
        lx.i += 2;
        std::size_t digits_start = lx.i;
        while (lx.i < lx.src.size() && (lx.src[lx.i] == '0' || lx.src[lx.i] == '1')) {
            ++lx.i;
        }
        if (lx.i == digits_start || (lx.i < lx.src.size() && is_ident_char(lx.src[lx.i]))) {
            return fail(lx, "bad binary number");
        }
        emit(lx, Token::Kind::Number, lx.src.substr(start, lx.i - start));
        return true;
    }

    while (lx.i < lx.src.size() && std::isdigit(static_cast<unsigned char>(lx.src[lx.i]))) {
        ++lx.i;
    }
    if (lx.i < lx.src.size() && is_ident_char(lx.src[lx.i])) {
        return fail(lx, "bad decimal number");
    }
    emit(lx, Token::Kind::Number, lx.src.substr(start, lx.i - start));
    return true;
}

Token* lex(const std::string& src, std::string& error) {
    Lexer lx{src, 0, 1, nullptr, nullptr, ""};
    if (src.size() >= 3 &&
        static_cast<unsigned char>(src[0]) == 0xEF &&
        static_cast<unsigned char>(src[1]) == 0xBB &&
        static_cast<unsigned char>(src[2]) == 0xBF) {
        lx.i = 3;
    }
    while (lx.i < src.size()) {
        char c = src[lx.i];
        if (c == ' ' || c == '\t' || c == '\r') {
            ++lx.i;
        } else if (c == ';') {
            while (lx.i < src.size() && src[lx.i] != '\n') {
                ++lx.i;
            }
        } else if (c == '\n') {
            emit(lx, Token::Kind::Newline, "\\n");
            ++lx.line;
            ++lx.i;
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = lx.i;
            while (lx.i < src.size() && is_ident_char(src[lx.i])) {
                ++lx.i;
            }
            emit(lx, Token::Kind::Ident, src.substr(start, lx.i - start));
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            if (!number(lx)) {
                break;
            }
        } else if (c == ',') {
            emit(lx, Token::Kind::Comma, ",");
            ++lx.i;
        } else if (c == ':') {
            emit(lx, Token::Kind::Colon, ":");
            ++lx.i;
        } else if (c == '[') {
            emit(lx, Token::Kind::LBracket, "[");
            ++lx.i;
        } else if (c == ']') {
            emit(lx, Token::Kind::RBracket, "]");
            ++lx.i;
        } else {
            fail(lx, std::string("unexpected character '") + c + "'");
            break;
        }
    }

    if (!lx.error.empty()) {
        destroy(lx.head);
        error = lx.error;
        return nullptr;
    }

    emit(lx, Token::Kind::Eof, "");
    return lx.head;
}