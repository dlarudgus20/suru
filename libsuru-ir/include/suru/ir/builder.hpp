#pragma once

#include <cstdint>

#include "suru/ir/codeunit.hpp"

namespace suru::ir {

using ConstantId = std::uint32_t;
using ChunkId = std::uint32_t;

class CodeUnitBuilder {
public:
    [[nodiscard]] ConstantId add_constant(Constant constant);
    [[nodiscard]] ChunkId add_chunk(Chunk chunk);
    [[nodiscard]] Chunk& chunk(ChunkId id);
    [[nodiscard]] CodeUnit finish(ChunkId entry);

private:
    CodeUnit unit_;
    bool finished_ {false};
};

} // namespace suru::ir
