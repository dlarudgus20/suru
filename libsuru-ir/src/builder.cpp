#include "suru/ir/builder.hpp"

#include <limits>
#include <utility>

#include "suru/ir/error.hpp"
#include "suru/ir/image.hpp"

namespace suru::ir {

ConstantId CodeUnitBuilder::add_constant(Constant constant) {
    if (finished_ || unit_.constants.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw ImageError("code unit builder is full or already finished");
    }
    unit_.constants.push_back(std::move(constant));
    return static_cast<ConstantId>(unit_.constants.size() - 1U);
}

ChunkId CodeUnitBuilder::add_chunk(Chunk chunk_value) {
    if (finished_ || unit_.chunks.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw ImageError("code unit builder is full or already finished");
    }
    unit_.chunks.push_back(std::move(chunk_value));
    return static_cast<ChunkId>(unit_.chunks.size() - 1U);
}

Chunk& CodeUnitBuilder::chunk(ChunkId id) {
    if (finished_ || id >= unit_.chunks.size()) throw ImageError("chunk id out of bounds");
    return unit_.chunks[id];
}

CodeUnit CodeUnitBuilder::finish(ChunkId entry) {
    if (finished_) throw ImageError("code unit builder already finished");
    unit_.entry_chunk = entry;
    validate(unit_);
    finished_ = true;
    return std::move(unit_);
}

} // namespace suru::ir
