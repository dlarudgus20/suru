#pragma once

#include <string_view>

#include "suru/ir/codeunit.hpp"
#include "suru/ir/error.hpp"

namespace suru::ir {

[[nodiscard]] CodeUnit assemble(std::string_view source);

} // namespace suru::ir
