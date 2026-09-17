#include "suru/ir/assembler.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "suru/ir/instruction.hpp"

namespace suru::ir {
namespace {

struct InstructionDef {
    std::string name;
    std::vector<std::string> args;
    std::size_t line {0};
};

struct ChunkDef {
    std::string name;
    std::uint8_t arity {0};
    std::uint8_t slots {0};
    std::vector<UpvalueInfo> upvalues;
    std::vector<InstructionDef> instructions;
};

[[noreturn]] void fail(std::size_t line, std::string message) {
    throw AssemblerError(line, std::move(message));
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

std::string_view remove_comment(std::string_view line) {
    char quote = '\0';
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quote != '\0') {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '\'' || ch == '"') quote = ch;
        else if (ch == ';') return line.substr(0, i);
        else if (ch == '-' && i + 1 < line.size() && line[i + 1] == '-') return line.substr(0, i);
    }
    return line;
}

std::vector<std::string> split_words(std::string_view line, std::size_t line_no) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        if (i == line.size()) break;
        const std::size_t start = i;
        if (line[i] == '\'' || line[i] == '"') {
            const char quote = line[i++];
            bool escaped = false;
            while (i < line.size()) {
                const char ch = line[i++];
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == quote) break;
            }
            if (i > line.size() || line[i - 1] != quote) fail(line_no, "unterminated string");
        } else {
            while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') ++i;
        }
        out.emplace_back(line.substr(start, i - start));
    }
    return out;
}

std::uint64_t parse_u64(std::string_view text, std::size_t line, std::string_view what) {
    std::uint64_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (ec != std::errc {} || ptr != text.data() + text.size()) {
        fail(line, "invalid " + std::string(what));
    }
    return value;
}

std::int64_t parse_i64(std::string_view text, std::size_t line, std::string_view what) {
    std::int64_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (ec != std::errc {} || ptr != text.data() + text.size()) {
        fail(line, "invalid " + std::string(what));
    }
    return value;
}

std::uint32_t unsigned_bits(
    std::string_view text, std::uint32_t max, std::size_t line, std::string_view what
) {
    const std::uint64_t value = parse_u64(text, line, what);
    if (value > max) fail(line, std::string(what) + " out of range");
    return static_cast<std::uint32_t>(value);
}

std::uint32_t signed_bits(
    std::string_view text, std::int64_t min, std::int64_t max,
    std::uint32_t mask, std::size_t line, std::string_view what
) {
    const std::int64_t value = parse_i64(text, line, what);
    if (value < min || value > max) fail(line, std::string(what) + " out of range");
    return static_cast<std::uint32_t>(value) & mask;
}

bool is_immediate(std::string_view text) {
    return !text.empty() && text.front() == '#';
}

std::string_view immediate_text(std::string_view text, std::size_t line) {
    if (!is_immediate(text) || text.size() == 1) fail(line, "invalid immediate");
    return text.substr(1);
}

std::uint32_t reg(std::string_view text, std::size_t line, std::string_view what = "register") {
    if (is_immediate(text)) fail(line, std::string(what) + " does not accept immediate");
    return unsigned_bits(text, 255, line, what);
}

std::string unquote(std::string_view text, std::size_t line) {
    if (text.size() < 2 || (text.front() != '"' && text.front() != '\'') || text.back() != text.front()) {
        fail(line, "expected quoted string");
    }
    std::string out;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\\' && i + 2 < text.size()) {
            ch = text[++i];
            switch (ch) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(ch); break;
            }
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

Op parse_op(std::string_view name, std::size_t line) {
    static const std::unordered_map<std::string_view, Op> ops {
        {"LOAD", Op::Load}, {"LOADNIL", Op::LoadNil}, {"LOADTRUE", Op::LoadTrue},
        {"LOADFALSE", Op::LoadFalse}, {"LOADK", Op::LoadK},
        {"GETGLOBALK", Op::GetGlobalK}, {"SETGLOBALK", Op::SetGlobalK},
        {"GETGLOBAL", Op::GetGlobal}, {"SETGLOBAL", Op::SetGlobal},
        {"ADD", Op::Add}, {"SUB", Op::Sub}, {"MUL", Op::Mul}, {"DIV", Op::Div},
        {"IDIV", Op::Idiv}, {"MOD", Op::Mod}, {"POW", Op::Pow},
        {"CONCAT", Op::Concat}, {"NEG", Op::Neg}, {"NOT", Op::Not}, {"LEN", Op::Len},
        {"AND", Op::And}, {"OR", Op::Or}, {"EQ", Op::Eq}, {"NE", Op::Ne},
        {"LT", Op::Lt}, {"LE", Op::Le}, {"GT", Op::Gt}, {"GE", Op::Ge},
        {"BAND", Op::Band}, {"BOR", Op::Bor}, {"BXOR", Op::Bxor},
        {"SHL", Op::Shl}, {"SHR", Op::Shr}, {"NEWTABLE", Op::NewTable},
        {"NEWARRAY", Op::NewArray}, {"GETINDEX", Op::GetIndex}, {"SETINDEX", Op::SetIndex},
        {"JMP", Op::Jmp}, {"IFFALSY", Op::IfFalsy}, {"IFTRUTHY", Op::IfTruthy},
        {"IFEQ", Op::IfEq}, {"IFNE", Op::IfNe}, {"IFLT", Op::IfLt},
        {"IFLE", Op::IfLe}, {"IFGT", Op::IfGt}, {"IFGE", Op::IfGe},
        {"CALL", Op::Call}, {"RETURN", Op::Return}, {"CLOSURE", Op::Closure},
        {"GETUPVAL", Op::GetUpvalue}, {"SETUPVAL", Op::SetUpvalue},
        {"VARGPREP", Op::VargPrep}, {"VARG", Op::Varg},
        {"PUSHARRAYX", Op::PushArrayX}, {"CLOSE", Op::Close}, {"RAISE", Op::Raise},
    };
    const auto it = ops.find(name);
    if (it == ops.end()) fail(line, "unknown opcode: " + std::string(name));
    return it->second;
}

std::uint32_t named_index(
    std::string_view text, const std::unordered_map<std::string, std::uint32_t>& names,
    std::uint32_t max, std::size_t line, std::string_view what
) {
    if (const auto it = names.find(std::string(text)); it != names.end()) return it->second;
    return unsigned_bits(text, max, line, what);
}

std::uint32_t return_count(std::string_view text, std::size_t line) {
    if (text == "@vret") return multret;
    const std::uint32_t value = unsigned_bits(text, multret, line, "return count");
    if (value == multret) fail(line, "return count must be 0..510 or @vret");
    return value;
}

bool binary_op(Op op) {
    switch (op) {
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Idiv:
        case Op::Mod:
        case Op::Pow:
        case Op::Concat:
        case Op::And:
        case Op::Or:
        case Op::Eq:
        case Op::Ne:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
        case Op::Band:
        case Op::Bor:
        case Op::Bxor:
        case Op::Shl:
        case Op::Shr:
            return true;
        default:
            return false;
    }
}

bool compare_skip_op(Op op) {
    return op >= Op::IfEq && op <= Op::IfGe;
}

bool immediate_binary(Op op) {
    return op != Op::And && op != Op::Or;
}

Word emit(
    const InstructionDef& inst,
    std::uint32_t offset,
    const std::unordered_map<std::string, std::uint32_t>& labels,
    const std::unordered_map<std::string, std::uint32_t>& constants,
    const std::unordered_map<std::string, std::uint32_t>& chunks
) {
    const bool open = inst.name.ends_with(".v");
    const std::string_view base = open
        ? std::string_view(inst.name).substr(0, inst.name.size() - 2) : std::string_view(inst.name);
    const Op op = parse_op(base, inst.line);
    const auto& a = inst.args;

    if (open) {
        if (op == Op::Call && a.size() == 2) {
            return encode_abc(op, reg(a[0], inst.line), 0, return_count(a[1], inst.line), true);
        }
        if ((op == Op::Return || op == Op::Varg) && a.size() == 1) {
            return encode_abx(op, reg(a[0], inst.line), 0, true);
        }
        if (op == Op::PushArrayX && a.size() == 2) {
            return encode_abc(op, reg(a[0], inst.line), reg(a[1], inst.line), 0, true);
        }
        fail(inst.line, "invalid .v instruction form");
    }

    if (op == Op::Load || op == Op::NewArray) {
        if (a.size() != 2) fail(inst.line, "opcode requires two operands");
        if (is_immediate(a[1])) {
            return encode_abx(op, reg(a[0], inst.line),
                signed_bits(immediate_text(a[1], inst.line), -65536, 65535, bx_mask, inst.line, "immediate"), true);
        }
        return encode_abx(op, reg(a[0], inst.line), reg(a[1], inst.line));
    }
    if (op == Op::Neg || op == Op::Not || op == Op::Len) {
        if (a.size() != 2) fail(inst.line, "opcode requires two operands");
        return encode_abx(op, reg(a[0], inst.line), reg(a[1], inst.line));
    }
    if (op == Op::LoadK) {
        if (a.size() != 2) fail(inst.line, "LOADK requires two operands");
        return encode_abx(op, reg(a[0], inst.line), named_index(a[1], constants, bx_mask, inst.line, "constant"));
    }
    if (op == Op::GetGlobalK) {
        if (a.size() != 2) fail(inst.line, "GETGLOBALK requires two operands");
        return encode_abx(op, named_index(a[0], constants, 255, inst.line, "constant"), reg(a[1], inst.line));
    }
    if (op == Op::SetGlobalK || op == Op::SetGlobal || op == Op::SetUpvalue) {
        if (a.size() != 2) fail(inst.line, "opcode requires two operands");
        const std::uint32_t first = op == Op::SetGlobalK
            ? named_index(a[0], constants, 255, inst.line, "constant") : reg(a[0], inst.line);
        if (is_immediate(a[1])) {
            return encode_abx(op, first,
                signed_bits(immediate_text(a[1], inst.line), -65536, 65535, bx_mask, inst.line, "immediate"), true);
        }
        return encode_abx(op, first, reg(a[1], inst.line));
    }
    if (op == Op::GetGlobal || op == Op::GetUpvalue) {
        if (a.size() != 2) fail(inst.line, "opcode requires two operands");
        return encode_abx(op, reg(a[0], inst.line), reg(a[1], inst.line));
    }
    if (op == Op::Closure) {
        if (a.size() != 2) fail(inst.line, "CLOSURE requires two operands");
        return encode_abx(op, reg(a[0], inst.line), named_index(a[1], chunks, bx_mask, inst.line, "chunk"));
    }
    if (op == Op::LoadNil || op == Op::LoadTrue || op == Op::LoadFalse
        || op == Op::NewTable || op == Op::Close || op == Op::Raise) {
        if (a.size() != 1) fail(inst.line, "opcode requires one operand");
        return encode_abx(op, reg(a[0], inst.line), 0);
    }
    if (binary_op(op)) {
        if (a.size() != 3) fail(inst.line, "opcode requires three operands");
        const std::uint32_t dst = reg(a[0], inst.line);
        const std::uint32_t lhs = reg(a[1], inst.line);
        if (is_immediate(a[2])) {
            if (!immediate_binary(op)) fail(inst.line, "logical opcode does not accept immediate");
            return encode_abc(op, dst, lhs,
                signed_bits(immediate_text(a[2], inst.line), -256, 255, c_mask, inst.line, "immediate"), true);
        }
        return encode_abc(op, dst, lhs, reg(a[2], inst.line));
    }
    if (op == Op::GetIndex) {
        if (a.size() != 3) fail(inst.line, "GETINDEX requires three operands");
        if (is_immediate(a[2])) {
            return encode_abc(op, reg(a[0], inst.line), reg(a[1], inst.line),
                signed_bits(immediate_text(a[2], inst.line), -256, 255, c_mask, inst.line, "index"), true);
        }
        return encode_abc(op, reg(a[0], inst.line), reg(a[1], inst.line), reg(a[2], inst.line));
    }
    if (op == Op::SetIndex) {
        if (a.size() != 3) fail(inst.line, "SETINDEX requires three operands");
        if (is_immediate(a[1])) {
            return encode_abc(op, reg(a[0], inst.line),
                signed_bits(immediate_text(a[1], inst.line), -128, 127, b_mask, inst.line, "index"),
                reg(a[2], inst.line), true);
        }
        return encode_abc(op, reg(a[0], inst.line), reg(a[1], inst.line), reg(a[2], inst.line));
    }
    if (op == Op::IfFalsy || op == Op::IfTruthy) {
        if (a.size() != 1) fail(inst.line, "opcode requires one operand");
        return encode_abx(op, reg(a[0], inst.line), 0);
    }
    if (compare_skip_op(op)) {
        if (a.size() != 2) fail(inst.line, "comparison requires two operands");
        if (is_immediate(a[1])) {
            return encode_abc(op, 0, reg(a[0], inst.line),
                signed_bits(immediate_text(a[1], inst.line), -256, 255, c_mask, inst.line, "immediate"), true);
        }
        return encode_abc(op, 0, reg(a[0], inst.line), reg(a[1], inst.line));
    }
    if (op == Op::Jmp) {
        if (a.size() != 1) fail(inst.line, "JMP requires one label");
        const auto it = labels.find(a[0]);
        if (it == labels.end()) fail(inst.line, "unknown label: " + a[0]);
        const std::int64_t relative = static_cast<std::int64_t>(it->second) - offset - 1;
        if (relative < -(1LL << 24) || relative > (1LL << 24) - 1) fail(inst.line, "jump out of range");
        return encode_sax(op, static_cast<std::int32_t>(relative));
    }
    if (op == Op::Call) {
        if (a.size() != 3) fail(inst.line, "CALL requires three operands");
        return encode_abc(op, reg(a[0], inst.line),
            unsigned_bits(a[1], 255, inst.line, "argument count"), return_count(a[2], inst.line));
    }
    if (op == Op::Return || op == Op::Varg) {
        if (a.size() != 2) fail(inst.line, "opcode requires two operands");
        return encode_abx(op, reg(a[0], inst.line), unsigned_bits(a[1], bx_mask, inst.line, "count"));
    }
    if (op == Op::VargPrep) {
        if (a.size() != 1) fail(inst.line, "VARGPREP requires one operand");
        return encode_abx(op, unsigned_bits(a[0], 255, inst.line, "fixed count"), 0);
    }
    if (op == Op::PushArrayX) {
        if (a.size() != 3) fail(inst.line, "PUSHARRAYX requires three operands");
        return encode_abc(op, reg(a[0], inst.line), reg(a[1], inst.line),
            unsigned_bits(a[2], c_mask, inst.line, "count"));
    }
    fail(inst.line, "unsupported opcode");
}

} // namespace

CodeUnit assemble(std::string_view source) {
    std::vector<Constant> constants;
    std::vector<std::string> constant_names;
    std::vector<ChunkDef> chunks;
    std::string entry_name;
    bool in_constants = false;
    ChunkDef* current = nullptr;

    std::size_t line_no = 0;
    while (!source.empty()) {
        ++line_no;
        const std::size_t newline = source.find('\n');
        std::string_view line = newline == std::string_view::npos ? source : source.substr(0, newline);
        source = newline == std::string_view::npos ? std::string_view {} : source.substr(newline + 1);
        line = trim(remove_comment(line));
        if (line.empty()) continue;
        std::vector<std::string> words = split_words(line, line_no);
        if (words.empty()) continue;

        if (words[0] == ".const") {
            if (words.size() != 1) fail(line_no, ".const does not accept operands");
            in_constants = true;
            current = nullptr;
            continue;
        }
        if (words[0] == ".entry") {
            if (words.size() != 2) fail(line_no, "expected .entry <chunk>");
            entry_name = words[1];
            continue;
        }
        if (words[0] == ".chunk") {
            if (words.size() != 4) fail(line_no, "expected .chunk <name> <arity> <slots>");
            const std::uint32_t slots = unsigned_bits(words[3], 255, line_no, "slot count");
            std::uint32_t arity = 255;
            if (words[2] != "@va") arity = unsigned_bits(words[2], 254, line_no, "arity");
            if (arity != 255 && arity > slots) fail(line_no, "chunk arity exceeds slots");
            chunks.push_back(ChunkDef {words[1], static_cast<std::uint8_t>(arity),
                static_cast<std::uint8_t>(slots), {}, {}});
            current = &chunks.back();
            in_constants = false;
            continue;
        }
        if (words[0] == ".upvalue") {
            if (current == nullptr || words.size() != 3) fail(line_no, "expected .upvalue <local|upvalue> <index>");
            UpvalueSource source_kind;
            if (words[1] == "local") source_kind = UpvalueSource::Local;
            else if (words[1] == "upvalue") source_kind = UpvalueSource::Upvalue;
            else fail(line_no, "upvalue source must be local or upvalue");
            const auto index = unsigned_bits(words[2], 255, line_no, "upvalue index");
            current->upvalues.push_back(UpvalueInfo {source_kind, static_cast<std::uint8_t>(index)});
            continue;
        }

        if (in_constants) {
            if (words.size() != 4 || words[1] != "=") fail(line_no, "invalid constant definition");
            constant_names.push_back(words[0]);
            if (words[2] == "number") {
                std::size_t pos = 0;
                double value = 0.0;
                try { value = std::stod(words[3], &pos); }
                catch (...) { fail(line_no, "invalid number constant"); }
                if (pos != words[3].size()) fail(line_no, "invalid number constant");
                constants.push_back(NumberConstant {value});
            } else if (words[2] == "string") {
                constants.push_back(StringConstant {unquote(words[3], line_no)});
            } else {
                fail(line_no, "unsupported constant type");
            }
            continue;
        }
        if (current == nullptr) fail(line_no, "expected .const or .chunk");
        if (words.size() == 1 && words[0].ends_with(':')) {
            words[0].pop_back();
            if (words[0].empty()) fail(line_no, "empty label");
            current->instructions.push_back(InstructionDef {"__LABEL__", {words[0]}, line_no});
            continue;
        }
        InstructionDef inst {words[0], {}, line_no};
        inst.args.assign(words.begin() + 1, words.end());
        current->instructions.push_back(std::move(inst));
    }

    if (chunks.empty()) fail(0, "at least one .chunk is required");
    std::unordered_map<std::string, std::uint32_t> constant_map;
    for (std::uint32_t i = 0; i < constant_names.size(); ++i) {
        if (!constant_map.emplace(constant_names[i], i).second) fail(0, "duplicate constant: " + constant_names[i]);
    }
    std::unordered_map<std::string, std::uint32_t> chunk_map;
    for (std::uint32_t i = 0; i < chunks.size(); ++i) {
        if (!chunk_map.emplace(chunks[i].name, i).second) fail(0, "duplicate chunk: " + chunks[i].name);
    }
    if (entry_name.empty()) entry_name = "main";
    const auto entry = chunk_map.find(entry_name);
    if (entry == chunk_map.end()) fail(0, "entry chunk not found: " + entry_name);

    CodeUnit unit;
    unit.constants = std::move(constants);
    unit.entry_chunk = entry->second;
    unit.chunks.reserve(chunks.size());
    for (const ChunkDef& def : chunks) {
        std::unordered_map<std::string, std::uint32_t> labels;
        std::uint32_t offset = 0;
        for (const InstructionDef& inst : def.instructions) {
            if (inst.name == "__LABEL__") {
                if (!labels.emplace(inst.args[0], offset).second) fail(inst.line, "duplicate label: " + inst.args[0]);
            } else {
                ++offset;
            }
        }
        Chunk chunk {def.name, def.arity, def.slots, def.upvalues, {}};
        chunk.code.reserve(offset);
        offset = 0;
        for (const InstructionDef& inst : def.instructions) {
            if (inst.name == "__LABEL__") continue;
            chunk.code.push_back(emit(inst, offset++, labels, constant_map, chunk_map));
        }
        unit.chunks.push_back(std::move(chunk));
    }
    return unit;
}

} // namespace suru::ir
