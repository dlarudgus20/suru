#include "suru/front/compiler.hpp"

#include <cctype>
#include <charconv>
#include <string>
#include <vector>

namespace suru::front {
namespace {

struct Token {
    std::string text;
    SourceLocation location;
};

std::vector<Token> tokenize(std::string_view source) {
    std::vector<Token> tokens;
    std::size_t line = 1;
    std::size_t column = 1;

    std::size_t i = 0;
    while (i < source.size()) {
        const char ch = source[i];
        if (ch == '\n') {
            ++line;
            column = 1;
            ++i;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            ++column;
            ++i;
            continue;
        }

        SourceLocation loc {line, column};
        if (ch == '+' || ch == ';') {
            tokens.push_back({std::string(1, ch), loc});
            ++column;
            ++i;
            continue;
        }

        std::size_t start = i;
        while (i < source.size()) {
            const char current = source[i];
            if (std::isspace(static_cast<unsigned char>(current)) != 0 || current == '+' || current == ';') {
                break;
            }
            ++i;
            ++column;
        }

        tokens.push_back({std::string(source.substr(start, i - start)), loc});
    }

    return tokens;
}

bool parse_i64(const std::string& token, std::int64_t& out) {
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc {} && ptr == end;
}

} // namespace

CompileResult compile(std::string_view source, const CompileOptions& options) {
    CompileResult result;
    auto tokens = tokenize(source);

    if (tokens.empty()) {
        result.diagnostics.push_back({{1, 1}, "empty source"});
        return result;
    }

    const std::string& mode = tokens[0].text;
    const bool is_print = mode == "print";
    const bool is_return = mode == "return";
    if (!is_print && !is_return) {
        result.diagnostics.push_back({tokens[0].location, "expected 'print' or 'return'"});
        return result;
    }

    std::size_t i = 1;
    bool expect_number = true;

    while (i < tokens.size()) {
        const auto& token = tokens[i];
        if (token.text == ";") {
            ++i;
            break;
        }

        if (expect_number) {
            std::int64_t value = 0;
            if (!parse_i64(token.text, value)) {
                result.diagnostics.push_back({token.location, "expected integer literal"});
                return result;
            }
            result.module.instructions.push_back({suru::vm::Opcode::ConstI64, value});
            expect_number = false;
            ++i;
            continue;
        }

        if (token.text != "+") {
            result.diagnostics.push_back({token.location, "expected '+' between literals"});
            return result;
        }
        ++i;

        if (i >= tokens.size()) {
            result.diagnostics.push_back({token.location, "expected integer after '+'"});
            return result;
        }

        const auto& number = tokens[i];
        std::int64_t value = 0;
        if (!parse_i64(number.text, value)) {
            result.diagnostics.push_back({number.location, "expected integer after '+'"});
            return result;
        }

        result.module.instructions.push_back({suru::vm::Opcode::ConstI64, value});
        result.module.instructions.push_back({suru::vm::Opcode::AddI64, 0});
        ++i;
    }

    if (result.module.instructions.empty()) {
        result.diagnostics.push_back({tokens[0].location, "expected at least one integer literal"});
        return result;
    }

    if (is_print && options.emit_print_for_print_stmt) {
        result.module.instructions.push_back({suru::vm::Opcode::PrintTop, 0});
    }
    result.module.instructions.push_back({suru::vm::Opcode::Halt, 0});

    return result;
}

} // namespace suru::front
