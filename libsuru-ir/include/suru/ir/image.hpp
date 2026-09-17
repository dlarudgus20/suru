#pragma once

#include <iosfwd>

#include "suru/ir/codeunit.hpp"
#include "suru/ir/error.hpp"

namespace suru::ir {

inline constexpr std::uint32_t image_magic = 0x43425300U;

void write_binary(std::ostream& out, const CodeUnit& unit);
[[nodiscard]] CodeUnit read_binary(std::istream& in);
void validate(const CodeUnit& unit);

} // namespace suru::ir
