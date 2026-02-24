#include "suru/front/parse.hpp"
#include "suru/front/parser.hpp"
#include "suru/front/lexer.hpp"

#include <utility>
#include <vector>

namespace suru::front {
namespace {

bool has_unclosed_delimiters(const std::vector<Token>& tokens) {
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    int block = 0;
    int repeat_block = 0;

    for (const Token& token : tokens) {
        switch (token.kind) {
            case TokenKind::LParen: ++paren; break;
            case TokenKind::RParen: if (paren > 0) --paren; break;
            case TokenKind::LBrace: ++brace; break;
            case TokenKind::RBrace: if (brace > 0) --brace; break;
            case TokenKind::LBracket: ++bracket; break;
            case TokenKind::RBracket: if (bracket > 0) --bracket; break;
            case TokenKind::KwFunction:
            case TokenKind::KwDo:
                ++block;
                break;
            case TokenKind::KwRepeat:
                ++repeat_block;
                break;
            case TokenKind::KwUntil:
                if (repeat_block > 0) {
                    --repeat_block;
                }
                break;
            case TokenKind::KwEnd:
                if (block > 0) {
                    --block;
                }
                break;
            default:
                break;
        }
    }

    return paren > 0 || brace > 0 || bracket > 0 || block > 0 || repeat_block > 0;
}

bool ends_with_continuation_token(const std::vector<Token>& tokens) {
    TokenKind last = TokenKind::EndOfFile;
    for (const Token& token : tokens) {
        if (token.kind != TokenKind::EndOfFile) {
            last = token.kind;
        }
    }

    switch (last) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::SlashSlash:
        case TokenKind::Percent:
        case TokenKind::Caret:
        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::ShiftLeft:
        case TokenKind::ShiftRight:
        case TokenKind::DotDot:
        case TokenKind::Less:
        case TokenKind::LessEq:
        case TokenKind::Greater:
        case TokenKind::GreaterEq:
        case TokenKind::EqEq:
        case TokenKind::NotEq:
        case TokenKind::Assign:
        case TokenKind::Comma:
        case TokenKind::Colon:
        case TokenKind::Dot:
        case TokenKind::KwAnd:
        case TokenKind::KwOr:
        case TokenKind::KwNot:
        case TokenKind::KwThen:
        case TokenKind::KwDo:
        case TokenKind::KwFunction:
        case TokenKind::KwLocal:
            return true;
        default:
            return false;
    }
}

bool looks_incomplete(const ParseResult& parse_result, const std::vector<Token>& tokens) {
    if (parse_result.ok()) {
        return false;
    }

    if (has_unclosed_delimiters(tokens) || ends_with_continuation_token(tokens)) {
        return true;
    }

    if (parse_result.diagnostics.empty() || tokens.empty()) {
        return false;
    }

    const SourceLocation eof_loc = tokens.back().location;
    const SourceLocation error_loc = parse_result.diagnostics.front().location;
    return error_loc.line == eof_loc.line && error_loc.column == eof_loc.column;
}

} // namespace

ParseResult parse(std::string_view source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    Parser parser(tokens);
    ParseResult parse_result = parser.run();

    if (looks_incomplete(parse_result, tokens)) {
        const SourceLocation eof_loc = tokens.empty() ? SourceLocation {1, 1} : tokens.back().location;
        parse_result.diagnostics.clear();
        parse_result.diagnostics.push_back({eof_loc, "unexpected end of file"});
    }
    return parse_result;
}

ParseSessionResult ParserSession::parse_fragment(std::string_view source_fragment) {
    if (!source_fragment.empty()) {
        buffer_.append(source_fragment.data(), source_fragment.size());
    }

    Lexer lexer(buffer_);
    std::vector<Token> tokens = lexer.tokenize();
    Parser parser(tokens);
    ParseResult parse_result = parser.run();

    ParseSessionResult out;
    out.tree = std::move(parse_result.tree);

    if (parse_result.ok()) {
        out.status = ParseStatus::Ok;
        buffer_.clear();
        return out;
    }

    if (looks_incomplete(parse_result, tokens)) {
        out.status = ParseStatus::Incomplete;
        return out;
    }

    out.status = ParseStatus::Error;
    out.diagnostics = std::move(parse_result.diagnostics);
    return out;
}

void ParserSession::reset() {
    buffer_.clear();
}

} // namespace suru::front
