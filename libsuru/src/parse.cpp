#include "suru/front/parse.hpp"
#include "suru/front/parser.hpp"
#include "suru/front/lexer.hpp"

#include <utility>
#include <vector>

namespace suru::front {

ParseResult parse(std::string_view source_fragment, ParseContext& context) {
    if (context.pending_source.empty()) {
        context.buffer_start = context.next_location;
    }
    context.pending_source += source_fragment;

    std::vector<Token> tokens = tokenize(context.pending_source, context.buffer_start);
    context.next_location = end_location(tokens);

    ParseResult parse_result = parse_tokens(std::move(tokens));

    if (parse_result.status != ParseStatus::Incomplete) {
        context.pending_source.clear();
        context.buffer_start = context.next_location;
    }

    parse_result.filename = context.filename;
    return parse_result;
}

ParseResult parse(std::string_view source, std::string_view filename) {
    ParseContext context {filename};
    return parse(source, context);
}

} // namespace suru::front
