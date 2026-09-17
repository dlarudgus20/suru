#include "suru/ir/image.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

namespace suru::ir {
namespace {

void write_u8(std::ostream& out, std::uint8_t value) {
    out.put(static_cast<char>(value));
    if (!out) throw ImageError("failed to write binary image");
}

void write_u32(std::ostream& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        write_u8(out, static_cast<std::uint8_t>(value >> shift));
    }
}

void write_f64(std::ostream& out, double value) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned shift = 0; shift < 64; shift += 8) {
        write_u8(out, static_cast<std::uint8_t>(bits >> shift));
    }
}

void write_string(std::ostream& out, std::string_view value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw ImageError("string is too large");
    }
    write_u32(out, static_cast<std::uint32_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!out) throw ImageError("failed to write binary image");
}

class Reader {
public:
    explicit Reader(std::istream& input) : input_(input) {}

    std::uint8_t u8() {
        const int ch = input_.get();
        if (ch == std::char_traits<char>::eof()) throw ImageError("unexpected end of binary image");
        return static_cast<std::uint8_t>(ch);
    }

    std::uint32_t u32() {
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) value |= std::uint32_t(u8()) << shift;
        return value;
    }

    double f64() {
        std::uint64_t bits = 0;
        for (unsigned shift = 0; shift < 64; shift += 8) bits |= std::uint64_t(u8()) << shift;
        return std::bit_cast<double>(bits);
    }

    std::string string() {
        const std::uint32_t size = u32();
        std::string value(size, '\0');
        input_.read(value.data(), static_cast<std::streamsize>(size));
        if (!input_) throw ImageError("unexpected end of binary image");
        return value;
    }

    bool at_end() {
        return input_.peek() == std::char_traits<char>::eof();
    }

private:
    std::istream& input_;
};

} // namespace

void validate(const CodeUnit& unit) {
    if (unit.chunks.empty() || unit.entry_chunk >= unit.chunks.size()) {
        throw ImageError("entry chunk index out of bounds");
    }
    if (unit.constants.size() > std::numeric_limits<std::uint32_t>::max()
        || unit.chunks.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw ImageError("code unit is too large");
    }
    std::unordered_set<std::string> names;
    for (const Chunk& chunk : unit.chunks) {
        if (chunk.name.empty()) throw ImageError("empty chunk name");
        if (!names.insert(chunk.name).second) throw ImageError("duplicate chunk name: " + chunk.name);
        if (chunk.upvalue_infos.size() > max_upvalue_count) throw ImageError("too many upvalues");
        if (chunk.arity != 255 && chunk.arity > chunk.slots) {
            throw ImageError("chunk arity exceeds slots");
        }
        if (chunk.code.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw ImageError("chunk is too large");
        }
        for (const UpvalueInfo info : chunk.upvalue_infos) {
            if (info.source != UpvalueSource::Local && info.source != UpvalueSource::Upvalue) {
                throw ImageError("unknown upvalue source");
            }
        }
        for (std::size_t pc = 0; pc < chunk.code.size(); ++pc) {
            const Word word = chunk.code[pc];
            const Op op = decode_op(word);
            if (static_cast<std::uint32_t>(op) > static_cast<std::uint32_t>(Op::Raise)) {
                throw ImageError("unknown opcode");
            }
            const WordABC abc = decode_abc(word);
            const WordABx abx = decode_abx(word);
            auto reg = [&](std::uint32_t value) {
                if (value >= chunk.slots) throw ImageError("register index out of chunk slot range");
            };
            auto constant = [&](std::uint32_t value) {
                if (value >= unit.constants.size()) throw ImageError("constant index out of bounds");
            };
            switch (op) {
                case Op::Load:
                    reg(abx.a); if (!abx.i) reg(abx.bx); break;
                case Op::LoadNil: case Op::LoadTrue: case Op::LoadFalse:
                case Op::NewTable: case Op::Raise:
                    reg(abx.a); break;
                case Op::LoadK:
                    reg(abx.a); constant(abx.bx); break;
                case Op::GetGlobalK:
                    constant(abx.a); reg(abx.bx); break;
                case Op::SetGlobalK:
                    constant(abx.a); if (!abx.i) reg(abx.bx); break;
                case Op::GetGlobal:
                    reg(abx.a); reg(abx.bx); break;
                case Op::SetGlobal:
                    reg(abx.a); if (!abx.i) reg(abx.bx); break;
                case Op::Add: case Op::Sub: case Op::Mul: case Op::Div:
                case Op::Idiv: case Op::Mod: case Op::Pow: case Op::Concat:
                case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le:
                case Op::Gt: case Op::Ge: case Op::Band: case Op::Bor:
                case Op::Bxor: case Op::Shl: case Op::Shr:
                    reg(abc.a); reg(abc.b); if (!abc.i) reg(abc.c); break;
                case Op::And: case Op::Or:
                    if (abc.i) throw ImageError("logical opcode does not accept immediate");
                    reg(abc.a); reg(abc.b); reg(abc.c); break;
                case Op::Neg: case Op::Not: case Op::Len:
                    reg(abx.a); reg(abx.bx); break;
                case Op::NewArray:
                    reg(abx.a); if (!abx.i) reg(abx.bx); break;
                case Op::GetIndex:
                    reg(abc.a); reg(abc.b); if (!abc.i) reg(abc.c); break;
                case Op::SetIndex:
                    reg(abc.a); if (!abc.i) reg(abc.b); reg(abc.c); break;
                case Op::Jmp: {
                    const std::int64_t target = static_cast<std::int64_t>(pc) + 1 + decode_sax(word).sax;
                    if (target < 0 || target > static_cast<std::int64_t>(chunk.code.size())) {
                        throw ImageError("jump target out of chunk bounds");
                    }
                    break;
                }
                case Op::IfFalsy: case Op::IfTruthy:
                    reg(abx.a); break;
                case Op::IfEq: case Op::IfNe: case Op::IfLt:
                case Op::IfLe: case Op::IfGt: case Op::IfGe:
                    reg(abc.b); if (!abc.i) reg(abc.c); break;
                case Op::Call:
                    reg(abc.a);
                    if (!abc.i && (abc.b > chunk.slots - abc.a - 1U)) throw ImageError("call argument range out of bounds");
                    if (abc.c != multret && abc.c > chunk.slots - abc.a) throw ImageError("call result range out of bounds");
                    break;
                case Op::Return:
                    if (abx.a > chunk.slots || (!abx.i && abx.bx > chunk.slots - abx.a)) {
                        throw ImageError("return range out of bounds");
                    }
                    break;
                case Op::Closure:
                    reg(abx.a);
                    if (abx.bx >= unit.chunks.size()) throw ImageError("chunk index out of bounds");
                    for (const UpvalueInfo capture : unit.chunks[abx.bx].upvalue_infos) {
                        if (capture.source == UpvalueSource::Local && capture.index >= chunk.slots) {
                            throw ImageError("captured local index out of parent chunk slot range");
                        }
                        if (capture.source == UpvalueSource::Upvalue
                            && capture.index >= chunk.upvalue_infos.size()) {
                            throw ImageError("captured upvalue index out of parent chunk range");
                        }
                    }
                    break;
                case Op::GetUpvalue:
                    if (abx.a >= chunk.upvalue_infos.size()) throw ImageError("upvalue index out of bounds");
                    reg(abx.bx); break;
                case Op::SetUpvalue:
                    if (abx.a >= chunk.upvalue_infos.size()) throw ImageError("upvalue index out of bounds");
                    if (!abx.i) reg(abx.bx);
                    break;
                case Op::VargPrep:
                    if (abx.a > chunk.slots) throw ImageError("vargprep range out of bounds");
                    break;
                case Op::Varg:
                    if (abx.a > chunk.slots || (!abx.i && abx.bx > chunk.slots - abx.a)) {
                        throw ImageError("varg range out of bounds");
                    }
                    break;
                case Op::PushArrayX:
                    reg(abc.a);
                    if (abc.b > chunk.slots || (!abc.i && abc.c > chunk.slots - abc.b)) {
                        throw ImageError("pusharrayx range out of bounds");
                    }
                    break;
                case Op::Close:
                    if (abx.a > chunk.slots) throw ImageError("close range out of bounds");
                    break;
            }
        }
    }
}

void write_binary(std::ostream& out, const CodeUnit& unit) {
    validate(unit);
    write_u32(out, image_magic);
    write_u32(out, static_cast<std::uint32_t>(unit.constants.size()));
    write_u32(out, static_cast<std::uint32_t>(unit.chunks.size()));
    write_u32(out, unit.entry_chunk);

    for (const Constant& constant : unit.constants) {
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, NumberConstant>) {
                write_u8(out, 1U);
                write_f64(out, value.value);
            } else {
                write_u8(out, 2U);
                write_string(out, value.value);
            }
        }, constant);
    }

    for (const Chunk& chunk : unit.chunks) {
        write_string(out, chunk.name);
        write_u8(out, chunk.arity);
        write_u8(out, chunk.slots);
        write_u8(out, static_cast<UpvalueCount>(chunk.upvalue_infos.size()));
        for (const UpvalueInfo info : chunk.upvalue_infos) {
            write_u8(out, static_cast<std::uint8_t>(info.source));
            write_u8(out, info.index);
        }
        write_u32(out, static_cast<std::uint32_t>(chunk.code.size()));
        for (const Word word : chunk.code) write_u32(out, word);
    }
}

CodeUnit read_binary(std::istream& in) {
    Reader rd(in);
    if (rd.u32() != image_magic) throw ImageError("invalid binary image magic");

    const std::uint32_t constant_count = rd.u32();
    const std::uint32_t chunk_count = rd.u32();
    CodeUnit unit;
    unit.entry_chunk = rd.u32();
    unit.constants.reserve(constant_count);
    unit.chunks.reserve(chunk_count);

    for (std::uint32_t i = 0; i < constant_count; ++i) {
        switch (rd.u8()) {
            case 1: unit.constants.push_back(NumberConstant {rd.f64()}); break;
            case 2: unit.constants.push_back(StringConstant {rd.string()}); break;
            default: throw ImageError("unsupported constant tag");
        }
    }

    for (std::uint32_t i = 0; i < chunk_count; ++i) {
        Chunk chunk;
        chunk.name = rd.string();
        chunk.arity = rd.u8();
        chunk.slots = rd.u8();
        const UpvalueCount upvalue_count = rd.u8();
        chunk.upvalue_infos.reserve(upvalue_count);
        for (std::size_t j = 0; j < upvalue_count; ++j) {
            const std::uint8_t source = rd.u8();
            if (source > static_cast<std::uint8_t>(UpvalueSource::Upvalue)) {
                throw ImageError("invalid upvalue source");
            }
            chunk.upvalue_infos.push_back(
                UpvalueInfo {static_cast<UpvalueSource>(source), rd.u8()});
        }
        const std::uint32_t code_count = rd.u32();
        chunk.code.reserve(code_count);
        for (std::uint32_t j = 0; j < code_count; ++j) chunk.code.push_back(rd.u32());
        unit.chunks.push_back(std::move(chunk));
    }

    if (!rd.at_end()) throw ImageError("extra trailing bytes in binary image");
    validate(unit);
    return unit;
}

} // namespace suru::ir
