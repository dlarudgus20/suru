#pragma once

#include <string>

#include "suru/front/location.hpp"

namespace suru::front {

enum class TokenKind {
    EndOfFile,
    Identifier,
    Numeral,
    String,
    UnterminatedString,
    VarArg,
    KwAnd,
    KwBreak,
    KwContinue,
    KwDo,
    KwElse,
    KwElseIf,
    KwEnd,
    KwFalse,
    KwFor,
    KwFunction,
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
    Pow,
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
    SourceRange range;
};

} // namespace suru::front
