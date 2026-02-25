#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <span>

#include "suru/vm/value.hpp"
#include "suru/vm/object.hpp"

namespace suru::vm {

class CodeUnit;

struct Chunk {
    std::span<char> opcodes_;
};

struct CodeUnit {
    std::vector<char> opcodes_;
    std::vector<Value> constants_;
    std::vector<Chunk> chunks_;
};

} // namespace suru::vm
