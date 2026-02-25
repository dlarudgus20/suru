#pragma once

#include <string_view>
#include <vector>

#include "suru/front/token.hpp"

namespace suru::front {

std::vector<Token> tokenize(std::string_view source, SourceLocation start_location = {});
SourceLocation end_location(const std::vector<Token>& tokens);

} // namespace suru::front
