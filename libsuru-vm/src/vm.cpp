#include "suru/vm/vm.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace suru::vm {
namespace {

constexpr std::array<char, 4> kMagic {'S', 'U', 'R', 'U'};

bool write_u8(std::ostream& out, std::uint8_t value) {
    out.put(static_cast<char>(value));
    return static_cast<bool>(out);
}

bool write_u32(std::ostream& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        if (!write_u8(out, static_cast<std::uint8_t>((value >> shift) & 0xFF))) {
            return false;
        }
    }
    return true;
}

bool write_i64(std::ostream& out, std::int64_t value) {
    const auto raw = static_cast<std::uint64_t>(value);
    for (int shift = 0; shift < 64; shift += 8) {
        if (!write_u8(out, static_cast<std::uint8_t>((raw >> shift) & 0xFF))) {
            return false;
        }
    }
    return true;
}

std::optional<std::uint8_t> read_u8(std::istream& in) {
    const int ch = in.get();
    if (ch == EOF) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(ch);
}

std::optional<std::uint32_t> read_u32(std::istream& in) {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        auto byte = read_u8(in);
        if (!byte) {
            return std::nullopt;
        }
        value |= static_cast<std::uint32_t>(*byte) << shift;
    }
    return value;
}

std::optional<std::int64_t> read_i64(std::istream& in) {
    std::uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        auto byte = read_u8(in);
        if (!byte) {
            return std::nullopt;
        }
        value |= static_cast<std::uint64_t>(*byte) << shift;
    }
    return static_cast<std::int64_t>(value);
}

const char* opcode_name(Opcode opcode) {
    switch (opcode) {
        case Opcode::Halt: return "HALT";
        case Opcode::ConstI64: return "CONST_I64";
        case Opcode::AddI64: return "ADD_I64";
        case Opcode::PrintTop: return "PRINT_TOP";
        default: return "UNKNOWN";
    }
}

bool has_operand(Opcode opcode) {
    return opcode == Opcode::ConstI64;
}

} // namespace

bool serialize_module(const BytecodeModule& module, std::ostream& out, std::string* error) {
    for (char c : kMagic) {
        out.put(c);
    }
    if (!write_u32(out, module.version)) {
        if (error) {
            *error = "failed to write module version";
        }
        return false;
    }

    if (!write_u32(out, static_cast<std::uint32_t>(module.instructions.size()))) {
        if (error) {
            *error = "failed to write instruction count";
        }
        return false;
    }

    for (const auto& instruction : module.instructions) {
        if (!write_u8(out, static_cast<std::uint8_t>(instruction.opcode))) {
            if (error) {
                *error = "failed to write opcode";
            }
            return false;
        }

        if (has_operand(instruction.opcode) && !write_i64(out, instruction.operand)) {
            if (error) {
                *error = "failed to write instruction operand";
            }
            return false;
        }
    }

    return static_cast<bool>(out);
}

std::optional<BytecodeModule> deserialize_module(std::istream& in, std::string* error) {
    for (char expected : kMagic) {
        const int ch = in.get();
        if (ch == EOF || static_cast<char>(ch) != expected) {
            if (error) {
                *error = "invalid bytecode magic";
            }
            return std::nullopt;
        }
    }

    auto version = read_u32(in);
    auto count = read_u32(in);
    if (!version || !count) {
        if (error) {
            *error = "truncated module header";
        }
        return std::nullopt;
    }

    BytecodeModule module;
    module.version = *version;
    module.instructions.reserve(*count);

    for (std::uint32_t i = 0; i < *count; ++i) {
        auto opcode_byte = read_u8(in);
        if (!opcode_byte) {
            if (error) {
                *error = "truncated opcode stream";
            }
            return std::nullopt;
        }

        Instruction instruction;
        instruction.opcode = static_cast<Opcode>(*opcode_byte);

        if (has_operand(instruction.opcode)) {
            auto operand = read_i64(in);
            if (!operand) {
                if (error) {
                    *error = "truncated instruction operand";
                }
                return std::nullopt;
            }
            instruction.operand = *operand;
        }

        module.instructions.push_back(instruction);
    }

    return module;
}

std::string disassemble(const BytecodeModule& module) {
    std::ostringstream out;
    out << "module_version " << module.version << '\n';
    for (std::size_t i = 0; i < module.instructions.size(); ++i) {
        const auto& instruction = module.instructions[i];
        out << std::setw(4) << i << "  " << opcode_name(instruction.opcode);
        if (has_operand(instruction.opcode)) {
            out << ' ' << instruction.operand;
        }
        out << '\n';
    }
    return out.str();
}

ExecutionResult execute(const BytecodeModule& module, const VMOptions& options) {
    ExecutionResult result;
    std::ostream* out = options.out != nullptr ? options.out : &std::cout;

    for (std::size_t pc = 0; pc < module.instructions.size(); ++pc) {
        const auto& instruction = module.instructions[pc];

        if (options.trace) {
            *out << "pc=" << pc << " op=" << opcode_name(instruction.opcode) << '\n';
        }

        switch (instruction.opcode) {
            case Opcode::Halt:
                result.final_stack = result.final_stack;
                return result;
            case Opcode::ConstI64:
                if (result.final_stack.size() >= options.max_stack) {
                    result.exit_code = 1;
                    result.error_message = "stack overflow";
                    return result;
                }
                result.final_stack.push_back(instruction.operand);
                break;
            case Opcode::AddI64:
                if (result.final_stack.size() < 2) {
                    result.exit_code = 1;
                    result.error_message = "stack underflow on ADD_I64";
                    return result;
                } {
                    const auto rhs = result.final_stack.back();
                    result.final_stack.pop_back();
                    const auto lhs = result.final_stack.back();
                    result.final_stack.back() = lhs + rhs;
                }
                break;
            case Opcode::PrintTop:
                if (result.final_stack.empty()) {
                    result.exit_code = 1;
                    result.error_message = "stack underflow on PRINT_TOP";
                    return result;
                }
                *out << result.final_stack.back() << '\n';
                break;
            default:
                result.exit_code = 1;
                result.error_message = "unknown opcode";
                return result;
        }
    }

    return result;
}

} // namespace suru::vm
