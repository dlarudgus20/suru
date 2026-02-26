#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "suru/vm/value.hpp"

namespace suru::vm {

struct Chunk {
    std::string name;
    std::uint32_t code_begin {0};
    std::uint32_t code_end {0};
    std::uint8_t arity {0};
    std::uint8_t slots {0};
    std::uint8_t upvalues {0};
};

struct CodeUnit {
    std::vector<std::uint8_t> opcodes_;
    std::vector<Value> constants_;
    std::vector<Chunk> chunks_;
};

} // namespace suru::vm

