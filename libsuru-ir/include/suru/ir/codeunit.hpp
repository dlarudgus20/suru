#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

#include "suru/ir/instruction.hpp"

namespace suru::ir {

using UpvalueCount = std::uint8_t;
inline constexpr UpvalueCount max_upvalue_count = std::numeric_limits<UpvalueCount>::max();

struct NumberConstant {
    double value {0.0};
};

struct StringConstant {
    std::string value;
};

using Constant = std::variant<NumberConstant, StringConstant>;

enum class UpvalueSource : std::uint8_t {
    Local = 0,
    Upvalue = 1,
};

struct UpvalueInfo {
    UpvalueSource source {UpvalueSource::Local};
    std::uint8_t index {0};
};

struct Chunk {
    std::string name;
    std::uint8_t arity {0};
    std::uint8_t slots {0};
    std::vector<UpvalueInfo> upvalue_infos;
    std::vector<Word> code;
};

struct CodeUnit {
    std::vector<Constant> constants;
    std::vector<Chunk> chunks;
    std::uint32_t entry_chunk {0};
};

} // namespace suru::ir
