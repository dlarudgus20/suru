#include <cctype>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "suru/lib/lib.hpp"
#include "suru/vm/opcode.hpp"
#include "suru/vm/vm.hpp"

namespace {

constexpr std::uint32_t kOpShift = 26U;
constexpr std::uint32_t kIShift = 25U;
constexpr std::uint32_t kIMask = 0x1U;
constexpr std::uint32_t kAxMask = 0x1FFFFFFU;
constexpr std::uint32_t kSbcMagic = 0x43425300U; // "\0SBC" in little-endian bytes
constexpr std::uint32_t kSbcVersion = 1U;

struct CompiledUnit {
    suru::vm::CodeUnit* code {nullptr};
    std::uint32_t entry_chunk_index {0};
};

struct CliOptions {
    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
};

struct AsmError : std::runtime_error {
    int line;
    AsmError(int line, std::string message) : std::runtime_error(std::move(message)), line(line) {}
};

struct CliError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string_view runtime_error_category_name(suru::vm::RuntimeErrorCategory category) {
    switch (category) {
        case suru::vm::RuntimeErrorCategory::Type: return "TypeError";
        case suru::vm::RuntimeErrorCategory::Api: return "ApiError";
        case suru::vm::RuntimeErrorCategory::Table: return "TableError";
        case suru::vm::RuntimeErrorCategory::InvalidCode: return "InvalidCodeError";
        case suru::vm::RuntimeErrorCategory::InvalidImage: return "InvalidImageError";
        case suru::vm::RuntimeErrorCategory::StackOverflow: return "StackOverflowError";
        case suru::vm::RuntimeErrorCategory::Internal: return "InternalError";
        default: return "RuntimeError";
    }
}

enum class Section {
    None,
    Const,
    Chunk,
};

struct ConstDef {
    std::string name;
    suru::vm::Value value;
};

struct InstDef {
    std::string op;
    std::vector<std::string> args;
    int line {0};
};

struct UpvalueInfoDef {
    suru::vm::UpvalueSource source {suru::vm::UpvalueSource::Local};
    std::uint8_t index {0};
    int line {0};
};

struct ChunkDef {
    std::string name;
    std::uint8_t arity {0};
    std::uint8_t slots {0};
    std::vector<UpvalueInfoDef> upvalue_infos;
    std::vector<InstDef> insts;
    std::unordered_set<std::string> label_names;
};

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::vector<std::string> split_ws(std::string_view text) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) != 0) {
            ++i;
        }
        if (i >= text.size()) {
            break;
        }
        const std::size_t begin = i;
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) == 0) {
            ++i;
        }
        out.emplace_back(text.substr(begin, i - begin));
    }
    return out;
}

std::string strip_comment(std::string_view line) {
    bool in_quote = false;
    char quote = '\0';
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (!in_quote && (ch == '"' || ch == '\'')) {
            in_quote = true;
            quote = ch;
            continue;
        }
        if (in_quote && ch == quote) {
            in_quote = false;
            continue;
        }
        if (!in_quote && ch == ';') {
            return std::string(line.substr(0, i));
        }
    }
    return std::string(line);
}

std::string unquote(std::string_view raw, int line) {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
        throw AsmError(line, "expected quoted string");
    }
    return std::string(raw.substr(1, raw.size() - 2));
}

std::uint64_t parse_u64(std::string_view text, int line, std::string_view what) {
    std::size_t pos = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(std::string(text), &pos, 10);
    } catch (...) {
        throw AsmError(line, std::string("invalid ") + std::string(what));
    }
    if (pos != text.size()) {
        throw AsmError(line, std::string("invalid ") + std::string(what));
    }
    return static_cast<std::uint64_t>(value);
}

std::int64_t parse_i64(std::string_view text, int line, std::string_view what) {
    std::size_t pos = 0;
    long long value = 0;
    try {
        value = std::stoll(std::string(text), &pos, 10);
    } catch (...) {
        throw AsmError(line, std::string("invalid ") + std::string(what));
    }
    if (pos != text.size()) {
        throw AsmError(line, std::string("invalid ") + std::string(what));
    }
    return static_cast<std::int64_t>(value);
}

std::uint32_t checked_u32(std::uint64_t value, int line, std::string_view what) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw AsmError(line, std::string(what) + " out of uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint8_t checked_u8(std::uint64_t value, int line, std::string_view what) {
    if (value > 0xFFU) {
        throw AsmError(line, std::string(what) + " out of 8-bit range");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint16_t checked_u9(std::uint64_t value, int line, std::string_view what) {
    if (value > 0x1FFU) {
        throw AsmError(line, std::string(what) + " out of 9-bit range");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint32_t checked_u18(std::uint64_t value, int line, std::string_view what) {
    if (value > 0x1FFFFU) {
        throw AsmError(line, std::string(what) + " out of 17-bit range");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t checked_s9_bits(std::int64_t value, int line, std::string_view what) {
    if (value < -256 || value > 255) {
        throw AsmError(line, std::string(what) + " out of signed 9-bit range");
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)) & 0x1FFU;
}

std::uint32_t checked_s17_bits(std::int64_t value, int line, std::string_view what) {
    if (value < -65536 || value > 65535) {
        throw AsmError(line, std::string(what) + " out of signed 17-bit range");
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)) & 0x1FFFFU;
}

std::uint32_t checked_s8_bits(std::int64_t value, int line, std::string_view what) {
    if (value < -128 || value > 127) {
        throw AsmError(line, std::string(what) + " out of signed 8-bit range");
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)) & 0xFFU;
}

std::int32_t checked_s26(std::int64_t value, int line, std::string_view what) {
    constexpr std::int64_t kMin = -(1LL << 24);
    constexpr std::int64_t kMax = (1LL << 24) - 1;
    if (value < kMin || value > kMax) {
        throw AsmError(line, std::string(what) + " out of signed 25-bit range");
    }
    return static_cast<std::int32_t>(value);
}

std::uint32_t pack_abc(suru::vm::Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return (static_cast<std::uint32_t>(op) << kOpShift)
        | ((a & 0xFFU) << 17U)
        | ((b & 0xFFU) << 9U)
        | (c & 0x1FFU);
}

std::uint32_t pack_abc_i(suru::vm::Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool i) {
    return (static_cast<std::uint32_t>(op) << kOpShift)
        | ((i ? kIMask : 0U) << kIShift)
        | ((a & 0xFFU) << 17U)
        | ((b & 0xFFU) << 9U)
        | (c & 0x1FFU);
}

std::uint32_t pack_abx(suru::vm::Op op, std::uint32_t a, std::uint32_t bx, bool i = false) {
    return (static_cast<std::uint32_t>(op) << kOpShift)
        | ((i ? kIMask : 0U) << kIShift)
        | ((a & 0xFFU) << 17U)
        | (bx & 0x1FFFFU);
}

std::uint32_t pack_sax(suru::vm::Op op, std::int32_t sx) {
    return (static_cast<std::uint32_t>(op) << kOpShift) | (static_cast<std::uint32_t>(sx) & kAxMask);
}

suru::vm::Op parse_op(std::string_view op, int line) {
    if (op == "JMPIF") {
        throw AsmError(line, "JMPIF removed; use IF* + JMP");
    }

    static const std::unordered_map<std::string, suru::vm::Op> kOps {
        {"LOAD", suru::vm::Op::Load},
        {"LOADNIL", suru::vm::Op::LoadNil},
        {"LOADTRUE", suru::vm::Op::LoadTrue},
        {"LOADFALSE", suru::vm::Op::LoadFalse},
        {"LOADK", suru::vm::Op::LoadK},
        {"GETGLOBALK", suru::vm::Op::GetGlobalK},
        {"SETGLOBALK", suru::vm::Op::SetGlobalK},
        {"GETGLOBAL", suru::vm::Op::GetGlobal},
        {"SETGLOBAL", suru::vm::Op::SetGlobal},
        {"ADD", suru::vm::Op::Add},
        {"SUB", suru::vm::Op::Sub},
        {"MUL", suru::vm::Op::Mul},
        {"DIV", suru::vm::Op::Div},
        {"IDIV", suru::vm::Op::Idiv},
        {"MOD", suru::vm::Op::Mod},
        {"POW", suru::vm::Op::Pow},
        {"CONCAT", suru::vm::Op::Concat},
        {"NEG", suru::vm::Op::Neg},
        {"NOT", suru::vm::Op::Not},
        {"LEN", suru::vm::Op::Len},
        {"AND", suru::vm::Op::And},
        {"OR", suru::vm::Op::Or},
        {"EQ", suru::vm::Op::Eq},
        {"NE", suru::vm::Op::Ne},
        {"LT", suru::vm::Op::Lt},
        {"LE", suru::vm::Op::Le},
        {"GT", suru::vm::Op::Gt},
        {"GE", suru::vm::Op::Ge},
        {"BAND", suru::vm::Op::Band},
        {"BOR", suru::vm::Op::Bor},
        {"BXOR", suru::vm::Op::Bxor},
        {"SHL", suru::vm::Op::Shl},
        {"SHR", suru::vm::Op::Shr},
        {"NEWTABLE", suru::vm::Op::NewTable},
        {"GETTABLE", suru::vm::Op::GetTable},
        {"SETTABLE", suru::vm::Op::SetTable},
        {"NEWARRAY", suru::vm::Op::NewArray},
        {"GETARRAY", suru::vm::Op::GetArray},
        {"SETARRAY", suru::vm::Op::SetArray},
        {"GETARRAYI", suru::vm::Op::GetArrayI},
        {"SETARRAYI", suru::vm::Op::SetArrayI},
        {"JMP", suru::vm::Op::Jmp},
        {"IFFALSY", suru::vm::Op::IfFalsy},
        {"IFTRUTHY", suru::vm::Op::IfTruthy},
        {"IFEQ", suru::vm::Op::IfEq},
        {"IFNE", suru::vm::Op::IfNe},
        {"IFLT", suru::vm::Op::IfLt},
        {"IFLE", suru::vm::Op::IfLe},
        {"IFGT", suru::vm::Op::IfGt},
        {"IFGE", suru::vm::Op::IfGe},
        {"CALL", suru::vm::Op::Call},
        {"RETURN", suru::vm::Op::Return},
        {"VARGPREP", suru::vm::Op::VargPrep},
        {"VARG", suru::vm::Op::Varg},
        {"CLOSURE", suru::vm::Op::Closure},
        {"GETUPVAL", suru::vm::Op::GetUpvalue},
        {"SETUPVAL", suru::vm::Op::SetUpvalue},
    };

    const auto it = kOps.find(std::string(op));
    if (it == kOps.end()) {
        throw AsmError(line, "unknown opcode: " + std::string(op));
    }
    return it->second;
}

std::uint32_t resolve_index(
    std::string_view token,
    int line,
    std::string_view what,
    const std::unordered_map<std::string, std::uint32_t>& map
) {
    const auto it = map.find(std::string(token));
    if (it != map.end()) {
        return it->second;
    }
    return checked_u32(parse_u64(token, line, what), line, what);
}

std::uint32_t emit_word(
    const InstDef& inst,
    std::uint32_t inst_offset,
    const std::unordered_map<std::string, std::uint32_t>& label_to_offset,
    const std::unordered_map<std::string, std::uint32_t>& const_index,
    const std::unordered_map<std::string, std::uint32_t>& chunk_index
) {
    const bool open = inst.op.ends_with(".v");
    const std::string_view name = inst.op;
    const suru::vm::Op op = parse_op(open ? name.substr(0, name.size() - 2U) : name, inst.line);
    if (open) {
        if (op == suru::vm::Op::Call && inst.args.size() == 2) {
            return pack_abc_i(op,
                checked_u8(parse_u64(inst.args[0], inst.line, "register"), inst.line, "register"), 0,
                checked_u9(parse_u64(inst.args[1], inst.line, "return count"), inst.line, "return count"), true);
        }
        if ((op == suru::vm::Op::Return || op == suru::vm::Op::Varg) && inst.args.size() == 1) {
            return pack_abx(op,
                checked_u8(parse_u64(inst.args[0], inst.line, "register"), inst.line, "register"), 0, true);
        }
        throw AsmError(inst.line, "expected CALL.v F retc, RETURN.v A, or VARG.v A");
    }

    auto is_immediate = [](std::string_view tok) {
        return !tok.empty() && tok.front() == '#';
    };

    auto parse_immediate = [&](std::string_view tok, std::string_view what) {
        if (!is_immediate(tok)) {
            throw AsmError(inst.line, std::string("expected immediate for ") + std::string(what));
        }
        if (tok.size() == 1) {
            throw AsmError(inst.line, std::string("missing immediate value for ") + std::string(what));
        }
        return parse_i64(tok.substr(1), inst.line, what);
    };

    auto immediate_enabled_abc = [&](suru::vm::Op v) {
        switch (v) {
            case suru::vm::Op::Add:
            case suru::vm::Op::Sub:
            case suru::vm::Op::Mul:
            case suru::vm::Op::Div:
            case suru::vm::Op::Idiv:
            case suru::vm::Op::Mod:
            case suru::vm::Op::Pow:
            case suru::vm::Op::Concat:
            case suru::vm::Op::Eq:
            case suru::vm::Op::Ne:
            case suru::vm::Op::Lt:
            case suru::vm::Op::Le:
            case suru::vm::Op::Gt:
            case suru::vm::Op::Ge:
            case suru::vm::Op::Band:
            case suru::vm::Op::Bor:
            case suru::vm::Op::Bxor:
            case suru::vm::Op::Shl:
            case suru::vm::Op::Shr:
            case suru::vm::Op::GetArray:
            case suru::vm::Op::SetArray:
            case suru::vm::Op::SetTable:
            case suru::vm::Op::IfEq:
            case suru::vm::Op::IfNe:
            case suru::vm::Op::IfLt:
            case suru::vm::Op::IfLe:
            case suru::vm::Op::IfGt:
            case suru::vm::Op::IfGe:
            case suru::vm::Op::SetArrayI:
                return true;
            default:
                return false;
        }
    };

    auto parse_i8_immediate = [&](std::string_view tok, std::string_view what) {
        if (!tok.empty() && tok.front() == '#') {
            if (tok.size() == 1) {
                throw AsmError(inst.line, std::string("missing immediate value for ") + std::string(what));
            }
            return checked_s8_bits(parse_i64(tok.substr(1), inst.line, what), inst.line, what);
        }
        return checked_s8_bits(parse_i64(tok, inst.line, what), inst.line, what);
    };

    auto parse_reg8 = [&](std::string_view tok, std::string_view what) {
        if (is_immediate(tok)) {
            throw AsmError(inst.line, std::string(what) + " does not accept immediate");
        }
        return static_cast<std::uint32_t>(checked_u8(parse_u64(tok, inst.line, what), inst.line, what));
    };
    auto parse_reg9 = [&](std::string_view tok, std::string_view what) {
        if (is_immediate(tok)) {
            throw AsmError(inst.line, std::string(what) + " does not accept immediate");
        }
        return static_cast<std::uint32_t>(checked_u9(parse_u64(tok, inst.line, what), inst.line, what));
    };

    switch (op) {
        case suru::vm::Op::Load:
        case suru::vm::Op::Neg:
        case suru::vm::Op::Not:
        case suru::vm::Op::Len: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "opcode requires two operands");
            }
            const std::uint32_t a = parse_reg8(inst.args[0], "register");
            if (op == suru::vm::Op::Load && is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s17_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abx(op, a, imm, true);
            }
            return pack_abx(op, a, checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register"));
        }
        case suru::vm::Op::NewArray: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "opcode requires two operands");
            }
            const std::uint32_t a = parse_reg8(inst.args[0], "register");
            if (is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s17_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abx(op, a, imm, true);
            }
            return pack_abx(op, a, checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register"));
        }
        case suru::vm::Op::GetUpvalue: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "GETUPVAL requires two operands");
            }
            const std::uint32_t upvalue = checked_u8(parse_u64(inst.args[0], inst.line, "upvalue index"), inst.line, "upvalue index");
            const std::uint32_t dst = parse_reg8(inst.args[1], "register");
            return pack_abx(op, upvalue, dst);
        }
        case suru::vm::Op::SetUpvalue: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "SETUPVAL requires two operands");
            }
            const std::uint32_t upvalue = checked_u8(parse_u64(inst.args[0], inst.line, "upvalue index"), inst.line, "upvalue index");
            if (is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s17_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abx(op, upvalue, imm, true);
            }
            const std::uint32_t src = checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register");
            return pack_abx(op, upvalue, src);
        }
        case suru::vm::Op::LoadK:
        case suru::vm::Op::GetGlobalK: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "opcode requires two operands");
            }
            if (op == suru::vm::Op::LoadK) {
                const std::uint32_t a = parse_reg8(inst.args[0], "register");
                const std::uint32_t idx = checked_u18(resolve_index(inst.args[1], inst.line, "index", const_index), inst.line, "index");
                return pack_abx(op, a, idx);
            }
            const std::uint32_t key = checked_u8(resolve_index(inst.args[0], inst.line, "index", const_index), inst.line, "index");
            const std::uint32_t dst = checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register");
            return pack_abx(op, key, dst);
        }
        case suru::vm::Op::SetGlobalK: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "SETGLOBALK requires two operands");
            }
            const std::uint32_t key = checked_u8(resolve_index(inst.args[0], inst.line, "index", const_index), inst.line, "index");
            if (is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s17_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abx(op, key, imm, true);
            }
            const std::uint32_t src = checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register");
            return pack_abx(op, key, src);
        }
        case suru::vm::Op::GetGlobal: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "GETGLOBAL requires two operands");
            }
            const std::uint32_t key_reg = parse_reg8(inst.args[0], "register");
            const std::uint32_t dst = checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register");
            return pack_abx(op, key_reg, dst);
        }
        case suru::vm::Op::SetGlobal: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "SETGLOBAL requires two operands");
            }
            const std::uint32_t key_reg = parse_reg8(inst.args[0], "register");
            if (is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s17_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abx(op, key_reg, imm, true);
            }
            const std::uint32_t src = checked_u18(parse_u64(inst.args[1], inst.line, "register"), inst.line, "register");
            return pack_abx(op, key_reg, src);
        }
        case suru::vm::Op::Closure: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "CLOSURE requires two operands");
            }
            const std::uint32_t a = parse_reg8(inst.args[0], "register");
            const std::uint32_t idx = checked_u18(resolve_index(inst.args[1], inst.line, "chunk", chunk_index), inst.line, "chunk index");
            return pack_abx(op, a, idx);
        }
        case suru::vm::Op::LoadNil:
        case suru::vm::Op::LoadTrue:
        case suru::vm::Op::LoadFalse:
        case suru::vm::Op::NewTable:
        case suru::vm::Op::IfFalsy:
        case suru::vm::Op::IfTruthy: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "opcode requires one operand");
            }
            return pack_abx(op, parse_reg8(inst.args[0], "register"), 0);
        }
        case suru::vm::Op::Add:
        case suru::vm::Op::Sub:
        case suru::vm::Op::Mul:
        case suru::vm::Op::Div:
        case suru::vm::Op::Idiv:
        case suru::vm::Op::Mod:
        case suru::vm::Op::Pow:
        case suru::vm::Op::Concat:
        case suru::vm::Op::And:
        case suru::vm::Op::Or:
        case suru::vm::Op::Eq:
        case suru::vm::Op::Ne:
        case suru::vm::Op::Lt:
        case suru::vm::Op::Le:
        case suru::vm::Op::Gt:
        case suru::vm::Op::Ge:
        case suru::vm::Op::Band:
        case suru::vm::Op::Bor:
        case suru::vm::Op::Bxor:
        case suru::vm::Op::Shl:
        case suru::vm::Op::Shr:
        case suru::vm::Op::GetTable:
        case suru::vm::Op::SetTable:
        case suru::vm::Op::GetArray:
        case suru::vm::Op::SetArray:
        case suru::vm::Op::Call: {
            if (inst.args.size() != 3) {
                throw AsmError(inst.line, "opcode requires three operands");
            }
            const std::uint32_t a = parse_reg8(inst.args[0], "operand A");
            const std::uint32_t b = parse_reg8(inst.args[1], "operand B");
            if (is_immediate(inst.args[2])) {
                if (!immediate_enabled_abc(op)) {
                    throw AsmError(inst.line, "immediate is not supported for this opcode");
                }
                const std::uint32_t imm = checked_s9_bits(parse_immediate(inst.args[2], "immediate"), inst.line, "immediate");
                return pack_abc_i(op, a, b, imm, true);
            }
            return pack_abc_i(
                op,
                a,
                b,
                parse_reg9(inst.args[2], "operand C"),
                false
            );
        }
        case suru::vm::Op::GetArrayI:
        case suru::vm::Op::SetArrayI: {
            if (inst.args.size() != 3) {
                throw AsmError(inst.line, "opcode requires three operands");
            }
            const std::uint32_t a = parse_reg8(inst.args[0], "operand A");
            const std::uint32_t b = parse_i8_immediate(inst.args[1], "operand B");
            if (op == suru::vm::Op::SetArrayI && is_immediate(inst.args[2])) {
                const std::uint32_t imm = checked_s9_bits(parse_immediate(inst.args[2], "immediate"), inst.line, "immediate");
                return pack_abc_i(op, a, b, imm, true);
            }
            return pack_abc_i(op, a, b, parse_reg9(inst.args[2], "operand C"), false);
        }
        case suru::vm::Op::IfEq:
        case suru::vm::Op::IfNe:
        case suru::vm::Op::IfLt:
        case suru::vm::Op::IfLe:
        case suru::vm::Op::IfGt:
        case suru::vm::Op::IfGe: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "skip compare opcode requires two operands");
            }
            const std::uint32_t b = parse_reg8(inst.args[0], "operand B");
            if (is_immediate(inst.args[1])) {
                const std::uint32_t imm = checked_s9_bits(parse_immediate(inst.args[1], "immediate"), inst.line, "immediate");
                return pack_abc_i(op, 0, b, imm, true);
            }
            return pack_abc_i(op, 0, b, parse_reg9(inst.args[1], "operand C"), false);
        }
        case suru::vm::Op::VargPrep: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "VARGPREP requires one operand");
            }
            return pack_abx(op, parse_reg8(inst.args[0], "fixed count"), 0);
        }
        case suru::vm::Op::Varg:
        case suru::vm::Op::Return: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "opcode requires two operands");
            }
            return pack_abx(op, parse_reg8(inst.args[0], "register"), checked_u18(parse_u64(inst.args[1], inst.line, "return count"), inst.line, "return count"));
        }
        case suru::vm::Op::Jmp: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "JMP requires one label");
            }
            const auto it = label_to_offset.find(inst.args[0]);
            if (it == label_to_offset.end()) {
                throw AsmError(inst.line, "unknown label: " + inst.args[0]);
            }
            const std::int64_t target = static_cast<std::int64_t>(it->second);
            const std::int64_t after = static_cast<std::int64_t>(inst_offset + 1U);
            const std::int32_t rel = checked_s26(target - after, inst.line, "jump offset");
            return pack_sax(op, rel);
        }
        default:
            throw AsmError(inst.line, "unsupported opcode");
    }
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path.string());
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    return ss.str();
}

std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size < 0) {
        throw std::runtime_error("failed to read file size: " + path.string());
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    if (size > 0) {
        input.read(reinterpret_cast<char*>(out.data()), size);
        if (!input) {
            throw std::runtime_error("failed to read file: " + path.string());
        }
    }
    return out;
}

void write_binary_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            throw std::runtime_error("failed to write output file: " + path.string());
        }
    }
}

bool starts_with_sbc_magic(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= 4
        && bytes[0] == 0x00
        && bytes[1] == 0x53
        && bytes[2] == 0x42
        && bytes[3] == 0x43;
}

void append_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

void append_f64(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u64(out, bits);
}

void append_string(std::vector<std::uint8_t>& out, std::string_view value) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

struct BinaryReader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t pos {0};

    std::uint8_t read_u8() {
        if (pos + 1 > bytes.size()) {
            throw std::runtime_error("unexpected end of file");
        }
        return bytes[pos++];
    }

    std::uint16_t read_u16() {
        const std::uint16_t b0 = read_u8();
        const std::uint16_t b1 = read_u8();
        return static_cast<std::uint16_t>(b0 | (b1 << 8U));
    }

    std::uint32_t read_u32() {
        const std::uint32_t b0 = read_u8();
        const std::uint32_t b1 = read_u8();
        const std::uint32_t b2 = read_u8();
        const std::uint32_t b3 = read_u8();
        return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
    }

    std::uint64_t read_u64() {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= (static_cast<std::uint64_t>(read_u8()) << (i * 8));
        }
        return value;
    }

    double read_f64() {
        const std::uint64_t bits = read_u64();
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::string read_string() {
        const std::uint32_t len = read_u32();
        if (pos + len > bytes.size()) {
            throw std::runtime_error("unexpected end of file");
        }
        std::string out(reinterpret_cast<const char*>(bytes.data() + pos), len);
        pos += len;
        return out;
    }
};

void print_help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru-bc <file.sura>\n"
        << "  suru-bc <file.sbc>\n"
        << "  suru-bc -o <out.sbc> <file.sura>\n"
        << "\n"
        << "Assembly format:\n"
        << "  .const\n"
        << "    k0 = number 1\n"
        << "    k1 = string \"print\"\n"
        << "  .chunk main 0 4\n"
        << "    GETGLOBALK k1 0\n"
        << "    CLOSURE 1 foo\n"
        << "    CALL 1 0 1\n"
        << "    CALL 0 1 0\n"
        << "    RETURN 0 0\n"
        << "  .chunk foo 0 2\n"
        << "    .upvalue local 0\n"
        << "    LOADK 0 k0\n"
        << "    RETURN 0 1\n";
}

CompiledUnit assemble_file(suru::vm::VM& vm, const std::filesystem::path& path) {
    Section section = Section::None;
    std::vector<ConstDef> const_defs;
    std::unordered_map<std::string, std::uint32_t> const_index;
    std::vector<ChunkDef> chunk_defs;
    ChunkDef* current_chunk = nullptr;

    const std::string source = read_text_file(path);
    std::istringstream input(source);
    std::string raw_line;
    int line_no = 0;

    while (std::getline(input, raw_line)) {
        ++line_no;
        const std::string no_comment = trim(strip_comment(raw_line));
        if (no_comment.empty()) {
            continue;
        }

        if (no_comment == ".const") {
            section = Section::Const;
            current_chunk = nullptr;
            continue;
        }

        if (no_comment.starts_with(".chunk")) {
            const auto parts = split_ws(no_comment);
            if (parts.size() != 4) {
                throw AsmError(line_no, "expected: .chunk <name> <arity> <slots>");
            }
            if (parts[1].empty()) {
                throw AsmError(line_no, "chunk name cannot be empty");
            }
            for (const ChunkDef& c : chunk_defs) {
                if (c.name == parts[1]) {
                    throw AsmError(line_no, "duplicate chunk name: " + parts[1]);
                }
            }
            ChunkDef chunk;
            chunk.name = parts[1];
            chunk.arity = parts[2] == "@va" ? 255
                : checked_u8(parse_u64(parts[2], line_no, "arity"), line_no, "arity");
            if (parts[2] != "@va" && chunk.arity == 255) {
                throw AsmError(line_no, "numeric arity must be <= 254; use @va for varargs");
            }
            chunk.slots = checked_u8(parse_u64(parts[3], line_no, "slots"), line_no, "slots");
            if (chunk.arity != 255 && chunk.arity > chunk.slots) {
                throw AsmError(line_no, "chunk arity must be <= slots");
            }
            chunk_defs.push_back(std::move(chunk));
            current_chunk = &chunk_defs.back();
            section = Section::Chunk;
            continue;
        }

        if (section == Section::None) {
            throw AsmError(line_no, "expected .const or .chunk");
        }

        if (section == Section::Const) {
            const std::size_t eq = no_comment.find('=');
            if (eq == std::string::npos) {
                throw AsmError(line_no, "invalid constant definition");
            }
            const std::string name = trim(no_comment.substr(0, eq));
            const std::string rhs = trim(no_comment.substr(eq + 1));
            const auto parts = split_ws(rhs);
            if (parts.empty()) {
                throw AsmError(line_no, "constant requires type");
            }
            if (const_index.contains(name)) {
                throw AsmError(line_no, "duplicate constant name: " + name);
            }

            suru::vm::Value value = suru::vm::Value::nil();
            if (parts[0] == "number") {
                if (parts.size() != 2) {
                    throw AsmError(line_no, "number constant requires value");
                }
                std::size_t pos = 0;
                double number = 0.0;
                try {
                    number = std::stod(parts[1], &pos);
                } catch (...) {
                    throw AsmError(line_no, "invalid number constant");
                }
                if (pos != parts[1].size()) {
                    throw AsmError(line_no, "invalid number constant");
                }
                value = suru::vm::Value::number(number);
            } else if (parts[0] == "string") {
                const std::string quoted = trim(rhs.substr(std::string("string").size()));
                value = suru::vm::Value::string(vm.make_string(unquote(quoted, line_no)));
            } else {
                throw AsmError(line_no, "constant type is not supported: " + parts[0]);
            }

            const_index.emplace(name, checked_u32(const_defs.size(), line_no, "constant index"));
            const_defs.push_back(ConstDef {name, value});
            continue;
        }

        if (section == Section::Chunk) {
            if (current_chunk == nullptr) {
                throw AsmError(line_no, "internal parser error: null current chunk");
            }

            if (no_comment.starts_with(".upvalue")) {
                const auto parts = split_ws(no_comment);
                if (parts.size() != 3) {
                    throw AsmError(line_no, "expected: .upvalue <local|upvalue> <index>");
                }
                UpvalueInfoDef info;
                if (parts[1] == "local") {
                    info.source = suru::vm::UpvalueSource::Local;
                } else if (parts[1] == "upvalue") {
                    info.source = suru::vm::UpvalueSource::Upvalue;
                } else {
                    throw AsmError(line_no, "upvalue source must be local or upvalue");
                }
                info.index = checked_u8(parse_u64(parts[2], line_no, "upvalue index"), line_no, "upvalue index");
                info.line = line_no;
                current_chunk->upvalue_infos.push_back(info);
                continue;
            }

            if (no_comment.back() == ':') {
                const std::string label = trim(no_comment.substr(0, no_comment.size() - 1));
                if (label.empty()) {
                    throw AsmError(line_no, "empty label");
                }
                if (current_chunk->label_names.contains(label)) {
                    throw AsmError(line_no, "duplicate label: " + label);
                }
                current_chunk->label_names.insert(label);
                current_chunk->insts.push_back(InstDef {"__LABEL__", {label}, line_no});
                continue;
            }

            auto parts = split_ws(no_comment);
            if (parts.empty()) {
                continue;
            }
            InstDef inst;
            inst.op = parts[0];
            inst.line = line_no;
            for (std::size_t i = 1; i < parts.size(); ++i) {
                inst.args.push_back(parts[i]);
            }
            current_chunk->insts.push_back(std::move(inst));
        }
    }

    if (chunk_defs.empty()) {
        throw AsmError(0, "at least one .chunk is required");
    }

    std::unordered_map<std::string, std::uint32_t> chunk_index;
    chunk_index.reserve(chunk_defs.size());
    for (std::size_t i = 0; i < chunk_defs.size(); ++i) {
        chunk_index.emplace(chunk_defs[i].name, checked_u32(i, 0, "chunk index"));
    }

    if (!chunk_index.contains("main")) {
        throw AsmError(0, "main chunk is required");
    }

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    cu->constants_.clear();
    cu->constants_.reserve(const_defs.size());
    for (const ConstDef& def : const_defs) {
        cu->constants_.push_back(def.value);
    }

    cu->code_.clear();
    cu->chunks_.clear();
    cu->chunks_.reserve(chunk_defs.size());

    for (ChunkDef& chunk : chunk_defs) {
        std::vector<suru::vm::UpvalueInfo> upvalue_infos;
        upvalue_infos.reserve(chunk.upvalue_infos.size());
        for (const UpvalueInfoDef& info : chunk.upvalue_infos) {
            if (info.source == suru::vm::UpvalueSource::Local) {
                if (info.index >= chunk.slots) {
                    throw AsmError(info.line, "local upvalue index out of chunk slot range");
                }
            }
            upvalue_infos.push_back(suru::vm::UpvalueInfo {info.source, info.index});
        }

        std::uint32_t local_offset = 0;
        std::unordered_map<std::string, std::uint32_t> label_offsets;
        for (const InstDef& inst : chunk.insts) {
            if (inst.op == "__LABEL__") {
                label_offsets[inst.args[0]] = local_offset;
                continue;
            }
            ++local_offset;
        }

        const std::uint32_t code_begin = checked_u32(cu->code_.size(), 0, "code begin");
        local_offset = 0;
        for (const InstDef& inst : chunk.insts) {
            if (inst.op == "__LABEL__") {
                continue;
            }
            cu->code_.push_back(emit_word(inst, local_offset, label_offsets, const_index, chunk_index));
            ++local_offset;
        }
        const std::uint32_t code_end = checked_u32(cu->code_.size(), 0, "code end");

        cu->chunks_.push_back(suru::vm::Chunk {
            chunk.name,
            code_begin,
            code_end,
            chunk.arity,
            chunk.slots,
            std::move(upvalue_infos),
        });
    }

    return CompiledUnit {cu, chunk_index.at("main")};
}

void write_sbc_file(const std::filesystem::path& path, const CompiledUnit& unit) {
    if (unit.code == nullptr) {
        throw std::runtime_error("cannot write sbc: null code unit");
    }

    std::vector<std::uint8_t> out;
    out.reserve(64 + unit.code->constants_.size() * 16 + unit.code->code_.size() * 4);

    append_u32(out, kSbcMagic);
    append_u32(out, kSbcVersion);
    append_u32(out, static_cast<std::uint32_t>(unit.code->constants_.size()));
    append_u32(out, static_cast<std::uint32_t>(unit.code->chunks_.size()));
    append_u32(out, static_cast<std::uint32_t>(unit.code->code_.size()));
    append_u32(out, unit.entry_chunk_index);

    for (const suru::vm::Value constant : unit.code->constants_) {
        if (constant.kind == suru::vm::ValueKind::Number) {
            append_u8(out, 1U);
            append_f64(out, constant.number_);
            continue;
        }
        if (constant.kind == suru::vm::ValueKind::String) {
            append_u8(out, 2U);
            append_string(out, constant.as_string("sbc write")->view());
            continue;
        }
        throw std::runtime_error("unsupported constant kind for sbc");
    }

    for (const suru::vm::Chunk& chunk : unit.code->chunks_) {
        append_string(out, chunk.name);
        append_u32(out, chunk.code_begin);
        append_u32(out, chunk.code_end);
        append_u8(out, chunk.arity);
        append_u8(out, chunk.slots);
        append_u16(out, static_cast<std::uint16_t>(chunk.upvalue_infos.size()));
        for (const suru::vm::UpvalueInfo info : chunk.upvalue_infos) {
            append_u8(out, static_cast<std::uint8_t>(info.source));
            append_u8(out, info.index);
        }
    }

    for (const std::uint32_t word : unit.code->code_) {
        append_u32(out, word);
    }

    write_binary_file(path, out);
}

CompiledUnit load_sbc_file(suru::vm::VM& vm, const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_binary_file(path);
    BinaryReader rd {bytes, 0};

    const std::uint32_t magic = rd.read_u32();
    if (magic != kSbcMagic) {
        throw std::runtime_error("invalid sbc magic");
    }
    const std::uint32_t version = rd.read_u32();
    if (version != kSbcVersion) {
        throw std::runtime_error("unsupported sbc version");
    }

    const std::uint32_t const_count = rd.read_u32();
    const std::uint32_t chunk_count = rd.read_u32();
    const std::uint32_t code_word_count = rd.read_u32();
    const std::uint32_t entry_chunk_index = rd.read_u32();

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    cu->constants_.clear();
    cu->constants_.reserve(const_count);
    for (std::uint32_t i = 0; i < const_count; ++i) {
        const std::uint8_t tag = rd.read_u8();
        if (tag == 1U) {
            cu->constants_.push_back(suru::vm::Value::number(rd.read_f64()));
            continue;
        }
        if (tag == 2U) {
            const std::string value = rd.read_string();
            cu->constants_.push_back(suru::vm::Value::string(vm.make_string(value)));
            continue;
        }
        throw std::runtime_error("unsupported sbc constant tag");
    }

    cu->chunks_.clear();
    cu->chunks_.reserve(chunk_count);
    for (std::uint32_t i = 0; i < chunk_count; ++i) {
        suru::vm::Chunk chunk;
        chunk.name = rd.read_string();
        chunk.code_begin = rd.read_u32();
        chunk.code_end = rd.read_u32();
        chunk.arity = rd.read_u8();
        chunk.slots = rd.read_u8();
        const std::uint16_t upvalue_count = rd.read_u16();
        chunk.upvalue_infos.clear();
        chunk.upvalue_infos.reserve(upvalue_count);
        for (std::uint16_t j = 0; j < upvalue_count; ++j) {
            const std::uint8_t source = rd.read_u8();
            const std::uint8_t index = rd.read_u8();
            if (source > 1U) {
                throw std::runtime_error("invalid upvalue source");
            }
            chunk.upvalue_infos.push_back(
                suru::vm::UpvalueInfo {
                    static_cast<suru::vm::UpvalueSource>(source),
                    index,
                }
            );
        }
        cu->chunks_.push_back(std::move(chunk));
    }

    cu->code_.clear();
    cu->code_.reserve(code_word_count);
    for (std::uint32_t i = 0; i < code_word_count; ++i) {
        cu->code_.push_back(rd.read_u32());
    }

    if (rd.pos != bytes.size()) {
        throw std::runtime_error("extra trailing bytes in sbc");
    }
    if (entry_chunk_index >= cu->chunks_.size()) {
        throw std::runtime_error("entry chunk index out of bounds");
    }
    for (const suru::vm::Chunk& chunk : cu->chunks_) {
        if (chunk.arity != 255 && chunk.arity > chunk.slots) {
            throw std::runtime_error("chunk arity exceeds slots");
        }
        if (chunk.code_begin > chunk.code_end || chunk.code_end > code_word_count) {
            throw std::runtime_error("chunk code range out of bounds");
        }
    }

    return CompiledUnit {cu, entry_chunk_index};
}

void run_compiled_unit(suru::vm::VM& vm, const CompiledUnit& unit) {
    if (unit.code == nullptr) {
        throw std::runtime_error("cannot execute null code unit");
    }
    if (unit.entry_chunk_index >= unit.code->chunks_.size()) {
        throw std::runtime_error("entry chunk index out of bounds");
    }
    suru::vm::Closure* entry = vm.make_closure(unit.code, unit.entry_chunk_index);
    vm.push_value(suru::vm::Value::closure(entry));
    vm.call(0, 0);
}

bool input_is_sbc(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_binary_file(path);
    return starts_with_sbc_magic(bytes);
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions options;
    if (argc == 2) {
        options.input = argv[1];
        return options;
    }
    if (argc == 4 && std::string_view(argv[1]) == "-o") {
        options.output = argv[2];
        options.input = argv[3];
        return options;
    }
    throw CliError("invalid arguments");
}

} // namespace

int main(int argc, char** argv) {
    std::string input_for_error = "<input>";
    try {
        if (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")) {
            print_help(std::cout);
            return 0;
        }

        const CliOptions options = parse_cli(argc, argv);
        input_for_error = options.input.string();

        suru::vm::VM vm;
        suru::lib::load_libs(vm);

        const bool is_sbc = input_is_sbc(options.input);
        if (options.output.has_value()) {
            if (is_sbc) {
                throw std::runtime_error("-o is only supported for assembly input");
            }
            const CompiledUnit unit = assemble_file(vm, options.input);
            write_sbc_file(*options.output, unit);
            return 0;
        }

        const CompiledUnit unit = is_sbc ? load_sbc_file(vm, options.input) : assemble_file(vm, options.input);
        run_compiled_unit(vm, unit);
        return 0;
    } catch (const AsmError& e) {
        if (e.line > 0) {
            std::cerr << input_for_error << ':' << e.line << ": error: " << e.what() << '\n';
        } else {
            std::cerr << "error: " << e.what() << '\n';
        }
        return 1;
    } catch (const CliError& e) {
        print_help(std::cerr);
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    } catch (const suru::vm::RuntimeError& e) {
        std::cerr << "runtime error [" << runtime_error_category_name(e.category()) << "]: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "runtime error: " << e.what() << '\n';
        return 1;
    }
}
