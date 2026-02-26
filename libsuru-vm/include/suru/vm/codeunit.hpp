#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "suru/vm/value.hpp"

namespace suru::vm {

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
    std::uint32_t code_begin {0};
    std::uint32_t code_end {0};
    std::uint8_t arity {0};
    std::uint8_t slots {0};
    std::vector<UpvalueInfo> upvalue_infos;
};

struct CodeUnit {
    std::vector<std::uint32_t> code_;
    std::vector<Value> constants_;
    std::vector<Chunk> chunks_;
};

} // namespace suru::vm

