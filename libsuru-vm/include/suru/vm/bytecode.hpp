#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace suru::vm {

enum class Opcode : std::uint8_t {
    Halt = 0x00,
    ConstI64 = 0x01,
    AddI64 = 0x02,
    PrintTop = 0x03,
};

struct Instruction {
    Opcode opcode {Opcode::Halt};
    std::int64_t operand {0};
};

struct BytecodeModule {
    std::uint32_t version {1};
    std::vector<Instruction> instructions;
};

bool serialize_module(const BytecodeModule& module, std::ostream& out, std::string* error = nullptr);
std::optional<BytecodeModule> deserialize_module(std::istream& in, std::string* error = nullptr);
std::string disassemble(const BytecodeModule& module);

} // namespace suru::vm
