#include "suru/ir/disassembler.hpp"

#include <cstdint>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

#include "suru/ir/error.hpp"
#include "suru/ir/image.hpp"
#include "suru/ir/instruction.hpp"

namespace suru::ir {
namespace {

std::string quote(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    out << '"';
    return out.str();
}

const char* name(Op op) {
    switch (op) {
        case Op::Load: return "LOAD";
        case Op::LoadNil: return "LOADNIL";
        case Op::LoadTrue: return "LOADTRUE";
        case Op::LoadFalse: return "LOADFALSE";
        case Op::LoadK: return "LOADK";
        case Op::GetGlobalK: return "GETGLOBALK";
        case Op::SetGlobalK: return "SETGLOBALK";
        case Op::Add: return "ADD";
        case Op::Sub: return "SUB";
        case Op::Mul: return "MUL";
        case Op::Div: return "DIV";
        case Op::Idiv: return "IDIV";
        case Op::Mod: return "MOD";
        case Op::Pow: return "POW";
        case Op::Concat: return "CONCAT";
        case Op::Neg: return "NEG";
        case Op::Not: return "NOT";
        case Op::Len: return "LEN";
        case Op::And: return "AND";
        case Op::Or: return "OR";
        case Op::Eq: return "EQ";
        case Op::Ne: return "NE";
        case Op::Lt: return "LT";
        case Op::Le: return "LE";
        case Op::Gt: return "GT";
        case Op::Ge: return "GE";
        case Op::Band: return "BAND";
        case Op::Bor: return "BOR";
        case Op::Bxor: return "BXOR";
        case Op::Shl: return "SHL";
        case Op::Shr: return "SHR";
        case Op::NewTable: return "NEWTABLE";
        case Op::NewArray: return "NEWARRAY";
        case Op::GetIndex: return "GETINDEX";
        case Op::SetIndex: return "SETINDEX";
        case Op::Jmp: return "JMP";
        case Op::IfFalsy: return "IFFALSY";
        case Op::IfTruthy: return "IFTRUTHY";
        case Op::IfEq: return "IFEQ";
        case Op::IfNe: return "IFNE";
        case Op::IfLt: return "IFLT";
        case Op::IfLe: return "IFLE";
        case Op::IfGt: return "IFGT";
        case Op::IfGe: return "IFGE";
        case Op::Call: return "CALL";
        case Op::Return: return "RETURN";
        case Op::Closure: return "CLOSURE";
        case Op::GetUpvalue: return "GETUPVAL";
        case Op::SetUpvalue: return "SETUPVAL";
        case Op::GetGlobal: return "GETGLOBAL";
        case Op::SetGlobal: return "SETGLOBAL";
        case Op::VargPrep: return "VARGPREP";
        case Op::Varg: return "VARG";
        case Op::PushArrayX: return "PUSHARRAYX";
        case Op::Close: return "CLOSE";
        case Op::Raise: return "RAISE";
    }
    throw ImageError("unknown opcode");
}

bool binary_op(Op op) {
    switch (op) {
        case Op::Add: case Op::Sub: case Op::Mul: case Op::Div:
        case Op::Idiv: case Op::Mod: case Op::Pow: case Op::Concat:
        case Op::And: case Op::Or: case Op::Eq: case Op::Ne:
        case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge:
        case Op::Band: case Op::Bor: case Op::Bxor: case Op::Shl: case Op::Shr:
            return true;
        default:
            return false;
    }
}

bool compare_skip_op(Op op) {
    return op >= Op::IfEq && op <= Op::IfGe;
}

std::string constant_name(std::uint32_t index, std::span<const Constant> constants) {
    if (index >= constants.size()) throw ImageError("constant index out of bounds");
    return "k" + std::to_string(index);
}

void instruction(
    std::ostream& out,
    Word word,
    std::uint32_t offset,
    std::span<const Constant> constants,
    const std::unordered_map<std::uint32_t, std::string>& labels,
    std::span<const Chunk> chunks
) {
    const Op op = decode_op(word);
    const WordABC abc = decode_abc(word);
    const WordABx abx = decode_abx(word);
    out << "    " << name(op);

    if (op == Op::Jmp) {
        const std::int64_t target = static_cast<std::int64_t>(offset) + 1 + decode_sax(word).sax;
        if (target < 0 || target > static_cast<std::int64_t>(UINT32_MAX)) {
            throw ImageError("jump target out of bounds");
        }
        const auto it = labels.find(static_cast<std::uint32_t>(target));
        if (it == labels.end()) throw ImageError("jump target out of chunk bounds");
        out << ' ' << it->second << '\n';
        return;
    }
    if (op == Op::Load || op == Op::NewArray) {
        out << ' ' << abx.a << ' ';
        if (abx.i) out << '#' << abx.imm_bx(); else out << abx.bx;
    } else if (op == Op::Neg || op == Op::Not || op == Op::Len
        || op == Op::GetGlobal || op == Op::GetUpvalue) {
        out << ' ' << abx.a << ' ' << abx.bx;
    } else if (op == Op::LoadK) {
        out << ' ' << abx.a << ' ' << constant_name(abx.bx, constants);
    } else if (op == Op::GetGlobalK) {
        out << ' ' << constant_name(abx.a, constants) << ' ' << abx.bx;
    } else if (op == Op::SetGlobalK) {
        out << ' ' << constant_name(abx.a, constants) << ' ';
        if (abx.i) out << '#' << abx.imm_bx(); else out << abx.bx;
    } else if (op == Op::SetGlobal || op == Op::SetUpvalue) {
        out << ' ' << abx.a << ' ';
        if (abx.i) out << '#' << abx.imm_bx(); else out << abx.bx;
    } else if (op == Op::Closure) {
        out << ' ' << abx.a << ' ';
        if (chunks.empty()) out << abx.bx;
        else {
            if (abx.bx >= chunks.size()) throw ImageError("chunk index out of bounds");
            out << chunks[abx.bx].name;
        }
    } else if (op == Op::LoadNil || op == Op::LoadTrue || op == Op::LoadFalse
        || op == Op::NewTable || op == Op::Close || op == Op::Raise) {
        out << ' ' << abx.a;
    } else if (binary_op(op)) {
        out << ' ' << abc.a << ' ' << abc.b << ' ';
        if (abc.i) out << '#' << abc.imm_c(); else out << abc.c;
    } else if (op == Op::GetIndex) {
        out << ' ' << abc.a << ' ' << abc.b << ' ';
        if (abc.i) out << '#' << abc.imm_c(); else out << abc.c;
    } else if (op == Op::SetIndex) {
        out << ' ' << abc.a << ' ';
        if (abc.i) out << '#' << abc.imm_b(); else out << abc.b;
        out << ' ' << abc.c;
    } else if (op == Op::IfFalsy || op == Op::IfTruthy) {
        out << ' ' << abx.a;
    } else if (compare_skip_op(op)) {
        out << ' ' << abc.b << ' ';
        if (abc.i) out << '#' << abc.imm_c(); else out << abc.c;
    } else if (op == Op::Call) {
        out << (abc.i ? ".v " : " ") << abc.a;
        if (!abc.i) out << ' ' << abc.b;
        out << ' ' << (abc.c == multret ? "@vret" : std::to_string(abc.c));
    } else if (op == Op::Return || op == Op::Varg) {
        out << (abx.i ? ".v " : " ") << abx.a;
        if (!abx.i) out << ' ' << abx.bx;
    } else if (op == Op::VargPrep) {
        out << ' ' << abx.a;
    } else if (op == Op::PushArrayX) {
        out << (abc.i ? ".v " : " ") << abc.a << ' ' << abc.b;
        if (!abc.i) out << ' ' << abc.c;
    } else {
        throw ImageError("unsupported opcode");
    }
    out << '\n';
}

void chunk_body(
    std::ostream& out,
    const Chunk& chunk,
    std::span<const Constant> constants,
    std::span<const Chunk> chunks
) {
    out << ".chunk " << chunk.name << ' ';
    if (chunk.arity == 255) out << "@va"; else out << static_cast<unsigned>(chunk.arity);
    out << ' ' << static_cast<unsigned>(chunk.slots) << '\n';
    for (const UpvalueInfo info : chunk.upvalue_infos) {
        out << ".upvalue " << (info.source == UpvalueSource::Local ? "local" : "upvalue")
            << ' ' << static_cast<unsigned>(info.index) << '\n';
    }

    std::unordered_map<std::uint32_t, std::string> labels;
    for (std::uint32_t offset = 0; offset < chunk.code.size(); ++offset) {
        if (decode_op(chunk.code[offset]) != Op::Jmp) continue;
        const std::int64_t target = static_cast<std::int64_t>(offset) + 1 + decode_sax(chunk.code[offset]).sax;
        if (target < 0 || target > static_cast<std::int64_t>(chunk.code.size())) {
            throw ImageError("jump target out of chunk bounds");
        }
        labels.try_emplace(static_cast<std::uint32_t>(target), "L" + std::to_string(target));
    }
    for (std::uint32_t offset = 0; offset < chunk.code.size(); ++offset) {
        if (const auto it = labels.find(offset); it != labels.end()) out << it->second << ":\n";
        instruction(out, chunk.code[offset], offset, constants, labels, chunks);
    }
    if (const auto it = labels.find(static_cast<std::uint32_t>(chunk.code.size())); it != labels.end()) {
        out << it->second << ":\n";
    }
}

} // namespace

void disassemble_chunk(std::ostream& out, const Chunk& chunk, std::span<const Constant> constants) {
    chunk_body(out, chunk, constants, {});
}

void disassemble_chunks(
    std::ostream& out, std::span<const Chunk> chunks, std::span<const Constant> constants
) {
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        if (i != 0) out << '\n';
        chunk_body(out, chunks[i], constants, chunks);
    }
}

void disassemble(std::ostream& out, const CodeUnit& unit) {
    validate(unit);
    out << ".const\n";
    for (std::size_t i = 0; i < unit.constants.size(); ++i) {
        out << "k" << i << " = ";
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, NumberConstant>) {
                out << "number " << std::setprecision(17) << value.value;
            } else {
                out << "string " << quote(value.value);
            }
        }, unit.constants[i]);
        out << '\n';
    }
    out << ".entry " << unit.chunks[unit.entry_chunk].name << "\n\n";
    disassemble_chunks(out, unit.chunks, unit.constants);
}

} // namespace suru::ir
