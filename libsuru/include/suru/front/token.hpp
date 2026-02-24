#pragma once

#include <cstddef>
#include <string>

namespace suru::front {

struct SourceLocation {
    std::size_t line {1};
    std::size_t column {1};
};

enum class TokenKind {
    EndOfFile,
    Identifier,
    Numeral,
    String,
    VarArg,
    KwAnd,
    KwBreak,
    KwDo,
    KwElse,
    KwElseIf,
    KwEnd,
    KwFalse,
    KwFor,
    KwFunction,
    KwGoto,
    KwIf,
    KwIn,
    KwLocal,
    KwNil,
    KwNot,
    KwOr,
    KwRepeat,
    KwReturn,
    KwThen,
    KwTrue,
    KwUntil,
    KwWhile,
    Plus,
    Minus,
    Star,
    Slash,
    SlashSlash,
    Percent,
    Caret,
    Hash,
    Amp,
    Pipe,
    ShiftLeft,
    ShiftRight,
    Tilde,
    Dot,
    DotDot,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    EqEq,
    NotEq,
    Assign,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Semicolon,
    Colon,
    ColonColon,
    Comma,
    Unknown,
};

struct Token {
    TokenKind kind {TokenKind::Unknown};
    std::string lexeme;
    SourceLocation location;
};

} // namespace suru::front
