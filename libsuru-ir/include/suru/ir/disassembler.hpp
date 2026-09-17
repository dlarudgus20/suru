#pragma once

#include <iosfwd>
#include <span>

#include "suru/ir/codeunit.hpp"

namespace suru::ir {

void disassemble_chunk(
    std::ostream& out,
    const Chunk& chunk,
    std::span<const Constant> constants
);

void disassemble_chunks(
    std::ostream& out,
    std::span<const Chunk> chunks,
    std::span<const Constant> constants
);

void disassemble(std::ostream& out, const CodeUnit& unit);

} // namespace suru::ir
