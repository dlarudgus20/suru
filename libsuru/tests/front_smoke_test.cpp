#include "suru/front/parse.hpp"
#include "suru/front/dump.hpp"
#include "suru/front/lexer.hpp"

#include <iostream>

int main() {
    auto result = suru::front::parse("return 1 + 2 + 3;");
    if (!result.ok()) {
        std::cerr << "parse unexpectedly failed\n";
        return 1;
    }

    const std::string dumped = suru::front::dump(result.tree);
    if (dumped.find("kind: 'Block'") == std::string::npos) {
        std::cerr << "parse tree missing block node\n";
        return 1;
    }

    if (dumped.find("kind: 'BinaryExpression'") == std::string::npos) {
        std::cerr << "parse tree missing binary expression\n";
        return 1;
    }

    if (dumped.find("value: '3'") == std::string::npos) {
        std::cerr << "parse tree missing numeral literal\n";
        return 1;
    }

    auto underscored_name = suru::front::parse("_x = 1;");
    if (!underscored_name.ok()) {
        std::cerr << "underscore-leading name should parse\n";
        return 1;
    }

    auto semicolon_stmt = suru::front::parse("break;");
    if (!semicolon_stmt.ok()) {
        std::cerr << "break statement with semicolon should parse\n";
        return 1;
    }
    const std::string semicolon_dumped = suru::front::dump(semicolon_stmt.tree);
    if (semicolon_dumped.find("kind: 'EmptyStatement'") != std::string::npos) {
        std::cerr << "semicolon should not create empty statement node\n";
        return 1;
    }

    auto no_long_string = suru::front::parse("return [[abc]]");
    if (no_long_string.ok()) {
        std::cerr << "long string syntax should not be accepted\n";
        return 1;
    }

    {
        suru::front::Lexer lexer("1e+ x");
        const auto tokens = lexer.tokenize();
        if (tokens.size() < 5 || tokens[0].kind != suru::front::TokenKind::Numeral || tokens[0].lexeme != "1" ||
            tokens[1].kind != suru::front::TokenKind::Identifier || tokens[1].location.column != 2 ||
            tokens[2].kind != suru::front::TokenKind::Plus || tokens[2].location.column != 3 ||
            tokens[3].kind != suru::front::TokenKind::Identifier || tokens[3].lexeme != "x" ||
            tokens[3].location.column != 5) {
            std::cerr << "exponent rollback location mismatch for '1e+ x'\n";
            return 1;
        }
    }

    {
        suru::front::Lexer lexer("1e x");
        const auto tokens = lexer.tokenize();
        if (tokens.size() < 4 || tokens[0].kind != suru::front::TokenKind::Numeral || tokens[0].lexeme != "1" ||
            tokens[1].kind != suru::front::TokenKind::Identifier || tokens[1].location.column != 2 ||
            tokens[2].kind != suru::front::TokenKind::Identifier || tokens[2].lexeme != "x" ||
            tokens[2].location.column != 4) {
            std::cerr << "exponent rollback location mismatch for '1e x'\n";
            return 1;
        }
    }

    {
        suru::front::Lexer lexer("1E- y");
        const auto tokens = lexer.tokenize();
        if (tokens.size() < 5 || tokens[0].kind != suru::front::TokenKind::Numeral || tokens[0].lexeme != "1" ||
            tokens[1].kind != suru::front::TokenKind::Identifier || tokens[1].location.column != 2 ||
            tokens[2].kind != suru::front::TokenKind::Minus || tokens[2].location.column != 3 ||
            tokens[3].kind != suru::front::TokenKind::Identifier || tokens[3].lexeme != "y" ||
            tokens[3].location.column != 5) {
            std::cerr << "exponent rollback location mismatch for '1E- y'\n";
            return 1;
        }
    }

    {
        suru::front::Lexer lexer("x = .1");
        const auto tokens = lexer.tokenize();
        if (tokens.size() < 5 || tokens[0].kind != suru::front::TokenKind::Identifier || tokens[0].lexeme != "x" ||
            tokens[2].kind != suru::front::TokenKind::Dot ||
            tokens[3].kind != suru::front::TokenKind::Numeral || tokens[3].lexeme != "1") {
            std::cerr << "leading-dot should tokenize as dot + numeral for 'x = .1'\n";
            return 1;
        }
    }

    auto leading_dot_parse = suru::front::parse("x = .1");
    if (leading_dot_parse.ok()) {
        std::cerr << "leading-dot numeral expression should be rejected\n";
        return 1;
    }

    auto eof_location = suru::front::parse("if x then\nreturn 1\n");
    if (eof_location.ok() || eof_location.diagnostics.empty()) {
        std::cerr << "expected eof parse diagnostic\n";
        return 1;
    }
    const auto& eof_diag = eof_location.diagnostics.front();
    if (eof_diag.message != "unexpected end of file" || eof_diag.location.line != 3 || eof_diag.location.column != 1) {
        std::cerr << "unexpected eof diagnostic location/message\n";
        return 1;
    }

    suru::front::ParserSession session;
    auto incomplete = session.parse_fragment("if x then\n");
    if (incomplete.status != suru::front::ParseStatus::Incomplete) {
        std::cerr << "expected incomplete status\n";
        return 1;
    }

    suru::front::ParserSession bad_elseif;
    auto bad_elseif_result = bad_elseif.parse_fragment("if x then elseif then end\n");
    if (bad_elseif_result.status != suru::front::ParseStatus::Error) {
        std::cerr << "elseif syntax error should not be incomplete\n";
        return 1;
    }

    auto recovered = session.parse_fragment("return x\nend\n");
    if (recovered.status != suru::front::ParseStatus::Ok) {
        std::cerr << "expected recovered parse success\n";
        return 1;
    }

    auto invalid = session.parse_fragment("local = 1\n");
    if (invalid.status != suru::front::ParseStatus::Error) {
        std::cerr << "expected parse error status\n";
        return 1;
    }

    return 0;
}
