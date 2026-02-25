#include "suru/front/lexer.hpp"

#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

namespace suru::front {
namespace {

const std::unordered_map<std::string, TokenKind>& keywords() {
    static const std::unordered_map<std::string, TokenKind> table {
        {"and", TokenKind::KwAnd},       {"break", TokenKind::KwBreak}, {"do", TokenKind::KwDo},
        {"else", TokenKind::KwElse},     {"elseif", TokenKind::KwElseIf},
        {"end", TokenKind::KwEnd},       {"false", TokenKind::KwFalse}, {"for", TokenKind::KwFor},
        {"fn", TokenKind::KwFunction},
        {"goto", TokenKind::KwGoto},     {"if", TokenKind::KwIf},       {"in", TokenKind::KwIn},
        {"local", TokenKind::KwLocal},   {"nil", TokenKind::KwNil},     {"not", TokenKind::KwNot},
        {"or", TokenKind::KwOr},         {"repeat", TokenKind::KwRepeat},
        {"return", TokenKind::KwReturn}, {"then", TokenKind::KwThen},   {"true", TokenKind::KwTrue},
        {"until", TokenKind::KwUntil},   {"while", TokenKind::KwWhile},
    };
    return table;
}

class Lexer {
public:
    explicit Lexer(std::string_view source, SourceLocation start_location) : source_(source), location_(start_location) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!at_end()) {
            skip_ignored();
            if (at_end()) {
                break;
            }
            tokens.push_back(scan_token());
        }
        tokens.push_back(Token {TokenKind::EndOfFile, "", location_});
        return tokens;
    }

private:
    bool at_end() const {
        return index_ >= source_.size();
    }

    char peek(std::size_t offset = 0) const {
        if (index_ + offset >= source_.size()) {
            return '\0';
        }
        return source_[index_ + offset];
    }

    char advance() {
        if (at_end()) {
            return '\0';
        }
        char ch = source_[index_++];
        if (ch == '\n') {
            ++location_.line;
            location_.column = 1;
        } else {
            ++location_.column;
        }
        return ch;
    }

    bool match(char expected) {
        if (peek() != expected) {
            return false;
        }
        advance();
        return true;
    }

    void skip_ignored() {
        while (!at_end()) {
            if (std::isspace(static_cast<unsigned char>(peek())) != 0) {
                advance();
                continue;
            }
            if (peek() == '-' && peek(1) == '-') {
                advance();
                advance();
                while (!at_end() && peek() != '\n') {
                    advance();
                }
                continue;
            }
            break;
        }
    }

    Token scan_token() {
    SourceLocation loc = location_;
    const char ch = peek();

    if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
        return scan_identifier_or_keyword(loc);
    }
    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        return scan_number(loc);
    }

    switch (ch) {
        case '\'':
        case '"': return scan_string(loc, ch);
        case '[':
            advance();
            return Token {TokenKind::LBracket, "[", loc};
        case ']': advance(); return Token {TokenKind::RBracket, "]", loc};
        case '{': advance(); return Token {TokenKind::LBrace, "{", loc};
        case '}': advance(); return Token {TokenKind::RBrace, "}", loc};
        case '(': advance(); return Token {TokenKind::LParen, "(", loc};
        case ')': advance(); return Token {TokenKind::RParen, ")", loc};
        case ';': advance(); return Token {TokenKind::Semicolon, ";", loc};
        case ',': advance(); return Token {TokenKind::Comma, ",", loc};
        case '+': advance(); return Token {TokenKind::Plus, "+", loc};
        case '-': advance(); return Token {TokenKind::Minus, "-", loc};
        case '*': advance(); return Token {TokenKind::Star, "*", loc};
        case '%': advance(); return Token {TokenKind::Percent, "%", loc};
        case '^':
            advance();
            if (match('^')) {
                return Token {TokenKind::Pow, "^^", loc};
            }
            return Token {TokenKind::Caret, "^", loc};
        case '#': advance(); return Token {TokenKind::Hash, "#", loc};
        case '&': advance(); return Token {TokenKind::Amp, "&", loc};
        case '|': advance(); return Token {TokenKind::Pipe, "|", loc};
        case '/':
            advance();
            if (match('/')) {
                return Token {TokenKind::SlashSlash, "//", loc};
            }
            return Token {TokenKind::Slash, "/", loc};
        case '=':
            advance();
            if (match('=')) {
                return Token {TokenKind::EqEq, "==", loc};
            }
            return Token {TokenKind::Assign, "=", loc};
        case '!':
            advance();
            if (match('=')) {
                return Token {TokenKind::NotEq, "!=", loc};
            }
            return Token {TokenKind::Unknown, "!", loc};
        case '~':
            advance();
            return Token {TokenKind::Tilde, "~", loc};
        case '<':
            advance();
            if (match('=')) {
                return Token {TokenKind::LessEq, "<=", loc};
            }
            if (match('<')) {
                return Token {TokenKind::ShiftLeft, "<<", loc};
            }
            return Token {TokenKind::Less, "<", loc};
        case '>':
            advance();
            if (match('=')) {
                return Token {TokenKind::GreaterEq, ">=", loc};
            }
            if (match('>')) {
                return Token {TokenKind::ShiftRight, ">>", loc};
            }
            return Token {TokenKind::Greater, ">", loc};
        case ':':
            advance();
            if (match(':')) {
                return Token {TokenKind::ColonColon, "::", loc};
            }
            return Token {TokenKind::Colon, ":", loc};
        case '.':
            advance();
            if (match('.')) {
                if (match('.')) {
                    return Token {TokenKind::VarArg, "...", loc};
                }
                return Token {TokenKind::DotDot, "..", loc};
            }
            return Token {TokenKind::Dot, ".", loc};
        default:
            advance();
            return Token {TokenKind::Unknown, std::string(1, ch), loc};
    }
}

    Token scan_identifier_or_keyword(SourceLocation loc) {
    const std::size_t start = index_;
    while (std::isalnum(static_cast<unsigned char>(peek())) != 0 || peek() == '_') {
        advance();
    }
    std::string text(source_.substr(start, index_ - start));
    const auto it = keywords().find(text);
    if (it != keywords().end()) {
        return Token {it->second, std::move(text), loc};
    }
    return Token {TokenKind::Identifier, std::move(text), loc};
}

    Token scan_number(SourceLocation loc) {
    const std::size_t start = index_;
    while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        advance();
    }
    if (peek() == '.' && peek(1) != '.') {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            advance();
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        const std::size_t save = index_;
        const SourceLocation save_location = location_;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        if (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                advance();
            }
        } else {
            index_ = save;
            location_ = save_location;
        }
    }
    return Token {TokenKind::Numeral, std::string(source_.substr(start, index_ - start)), loc};
}

    Token scan_string(SourceLocation loc, char quote) {
    advance();
    std::string value;
    while (!at_end()) {
        const char ch = advance();
        if (ch == quote) {
            return Token {TokenKind::String, std::move(value), loc};
        }
        if (ch == '\\' && !at_end()) {
            value.push_back(advance());
        } else {
            value.push_back(ch);
        }
    }
    return Token {TokenKind::Unknown, std::move(value), loc};
}

    std::string_view source_;
    std::size_t index_ {0};
    SourceLocation location_ {};
};

} // namespace

std::vector<Token> tokenize(std::string_view source, SourceLocation start_location) {
    return Lexer(source, start_location).tokenize();
}

SourceLocation end_location(const std::vector<Token>& tokens) {
    if (tokens.empty()) {
        return SourceLocation {};
    }
    return tokens.back().location;
}

} // namespace suru::front
