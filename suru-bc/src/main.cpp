#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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

struct AsmError : std::runtime_error {
    int line;
    AsmError(int line, std::string message) : std::runtime_error(std::move(message)), line(line) {}
};

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

struct ChunkDef {
    std::string name;
    std::size_t arity {0};
    std::size_t slots {0};
    std::size_t upvalues {0};
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

void append_uleb(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (true) {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0) {
            byte |= 0x80U;
        }
        out.push_back(byte);
        if (value == 0) {
            break;
        }
    }
}

void append_sleb32_fixed(std::vector<std::uint8_t>& out, std::int32_t value) {
    std::int64_t sv = static_cast<std::int64_t>(value);
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(sv) & 0x7fU) | 0x80U));
        sv >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(sv) & 0x7fU));
}

std::size_t encoded_uleb_size(std::uint64_t value) {
    std::size_t count = 0;
    do {
        value >>= 7U;
        ++count;
    } while (value != 0);
    return count;
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

suru::vm::Op parse_op(std::string_view op, int line) {
    static const std::unordered_map<std::string, suru::vm::Op> kOps {
        {"POP", suru::vm::Op::Pop},
        {"NIL", suru::vm::Op::Nil},
        {"TRUE", suru::vm::Op::True},
        {"FALSE", suru::vm::Op::False},
        {"CONST", suru::vm::Op::Const},
        {"GET_LOCAL", suru::vm::Op::GetLocal},
        {"SET_LOCAL", suru::vm::Op::SetLocal},
        {"GET_GLOBAL", suru::vm::Op::GetGlobal},
        {"SET_GLOBAL", suru::vm::Op::SetGlobal},
        {"ADD", suru::vm::Op::Add},
        {"SUB", suru::vm::Op::Sub},
        {"MUL", suru::vm::Op::Mul},
        {"DIV", suru::vm::Op::Div},
        {"IDIV", suru::vm::Op::Idiv},
        {"MOD", suru::vm::Op::Mod},
        {"POW", suru::vm::Op::Pow},
        {"NEG", suru::vm::Op::Neg},
        {"NOT", suru::vm::Op::Not},
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
        {"NEW_TABLE", suru::vm::Op::NewTable},
        {"GET_TABLE", suru::vm::Op::GetTable},
        {"SET_TABLE", suru::vm::Op::SetTable},
        {"JMP", suru::vm::Op::Jmp},
        {"JMP_IF_FALSE", suru::vm::Op::JmpIfFalse},
        {"CALL", suru::vm::Op::Call},
        {"RETURN", suru::vm::Op::Return},
        {"CLOSURE", suru::vm::Op::Closure},
        {"GET_UPVALUE", suru::vm::Op::GetUpvalue},
        {"SET_UPVALUE", suru::vm::Op::SetUpvalue},
    };

    const auto it = kOps.find(std::string(op));
    if (it == kOps.end()) {
        throw AsmError(line, "unknown opcode: " + std::string(op));
    }
    return it->second;
}

std::size_t inst_size(
    const InstDef& inst,
    const std::unordered_map<std::string, std::size_t>& const_index,
    const std::unordered_map<std::string, std::size_t>& chunk_index
) {
    const suru::vm::Op op = parse_op(inst.op, inst.line);
    switch (op) {
        case suru::vm::Op::Const:
        case suru::vm::Op::GetLocal:
        case suru::vm::Op::SetLocal:
        case suru::vm::Op::GetGlobal:
        case suru::vm::Op::SetGlobal:
        case suru::vm::Op::GetUpvalue:
        case suru::vm::Op::SetUpvalue:
        case suru::vm::Op::Return: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "opcode requires one operand");
            }
            const auto it = const_index.find(inst.args[0]);
            if (it != const_index.end() && (op == suru::vm::Op::Const || op == suru::vm::Op::GetGlobal || op == suru::vm::Op::SetGlobal)) {
                return 1U + encoded_uleb_size(it->second);
            }
            return 1U + encoded_uleb_size(parse_u64(inst.args[0], inst.line, "integer"));
        }
        case suru::vm::Op::Call: {
            if (inst.args.size() != 2) {
                throw AsmError(inst.line, "CALL requires two operands");
            }
            return 1U + encoded_uleb_size(parse_u64(inst.args[0], inst.line, "arg_count"))
                + encoded_uleb_size(parse_u64(inst.args[1], inst.line, "ret_count"));
        }
        case suru::vm::Op::Jmp:
        case suru::vm::Op::JmpIfFalse: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "jump opcode requires one label operand");
            }
            return 1U + 5U;
        }
        case suru::vm::Op::Closure: {
            if (inst.args.size() != 1) {
                throw AsmError(inst.line, "CLOSURE requires one chunk operand");
            }
            const auto it = chunk_index.find(inst.args[0]);
            if (it == chunk_index.end()) {
                throw AsmError(inst.line, "unknown chunk name: " + inst.args[0]);
            }
            return 1U + encoded_uleb_size(it->second);
        }
        default:
            if (!inst.args.empty()) {
                throw AsmError(inst.line, "opcode does not take operands");
            }
            return 1U;
    }
}

void emit_inst(
    std::vector<std::uint8_t>& out,
    const InstDef& inst,
    std::size_t inst_offset,
    const std::unordered_map<std::string, std::size_t>& label_to_offset,
    const std::unordered_map<std::string, std::size_t>& const_index,
    const std::unordered_map<std::string, std::size_t>& chunk_index
) {
    const suru::vm::Op op = parse_op(inst.op, inst.line);
    out.push_back(static_cast<std::uint8_t>(op));

    auto parse_operand_index = [&](std::string_view text) -> std::size_t {
        const auto it = const_index.find(std::string(text));
        if (it != const_index.end()) {
            return it->second;
        }
        return static_cast<std::size_t>(parse_u64(text, inst.line, "index"));
    };

    switch (op) {
        case suru::vm::Op::Const:
        case suru::vm::Op::GetLocal:
        case suru::vm::Op::SetLocal:
        case suru::vm::Op::GetGlobal:
        case suru::vm::Op::SetGlobal:
        case suru::vm::Op::GetUpvalue:
        case suru::vm::Op::SetUpvalue:
        case suru::vm::Op::Return: {
            append_uleb(out, static_cast<std::uint64_t>(parse_operand_index(inst.args[0])));
            break;
        }
        case suru::vm::Op::Call: {
            append_uleb(out, parse_u64(inst.args[0], inst.line, "arg_count"));
            append_uleb(out, parse_u64(inst.args[1], inst.line, "ret_count"));
            break;
        }
        case suru::vm::Op::Jmp:
        case suru::vm::Op::JmpIfFalse: {
            const auto it = label_to_offset.find(inst.args[0]);
            if (it == label_to_offset.end()) {
                throw AsmError(inst.line, "unknown label: " + inst.args[0]);
            }
            const std::int64_t after = static_cast<std::int64_t>(inst_offset + 1U + 5U);
            const std::int64_t target = static_cast<std::int64_t>(it->second);
            const std::int64_t rel = target - after;
            append_sleb32_fixed(out, static_cast<std::int32_t>(rel));
            break;
        }
        case suru::vm::Op::Closure: {
            const auto it = chunk_index.find(inst.args[0]);
            if (it == chunk_index.end()) {
                throw AsmError(inst.line, "unknown chunk name: " + inst.args[0]);
            }
            append_uleb(out, static_cast<std::uint64_t>(it->second));
            break;
        }
        default:
            break;
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

void print_help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru-bc <file.sura>\n"
        << "\n"
        << "Assembly format:\n"
        << "  .const\n"
        << "    k0 = number 1\n"
        << "    k1 = string \"print\"\n"
        << "  .chunk main 0 16 0\n"
        << "    GET_GLOBAL k1\n"
        << "    CLOSURE foo\n"
        << "    GET_UPVALUE 0\n"
        << "    SET_UPVALUE 0\n"
        << "    CALL 0 1\n"
        << "    CALL 1 0\n"
        << "    RETURN 0\n"
        << "  .chunk foo 0 8 0\n"
        << "    CONST k0\n"
        << "    RETURN 1\n";
}

int run_file(const std::filesystem::path& path) {
    suru::vm::VM vm;
    suru::lib::load_libs(vm);

    Section section = Section::None;
    std::vector<ConstDef> const_defs;
    std::unordered_map<std::string, std::size_t> const_index;
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
            if (parts.size() != 5) {
                throw AsmError(line_no, "expected: .chunk <name> <arity> <slots> <upvalues>");
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
            chunk.arity = static_cast<std::size_t>(parse_u64(parts[2], line_no, "arity"));
            chunk.slots = static_cast<std::size_t>(parse_u64(parts[3], line_no, "slots"));
            chunk.upvalues = static_cast<std::size_t>(parse_u64(parts[4], line_no, "upvalues"));
            if (chunk.arity > chunk.slots) {
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
            if (parts.size() < 1) {
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
            } else if (parts[0] == "nil") {
                value = suru::vm::Value::nil();
            } else if (parts[0] == "true") {
                value = suru::vm::Value::boolean(true);
            } else if (parts[0] == "false") {
                value = suru::vm::Value::boolean(false);
            } else {
                throw AsmError(line_no, "unknown constant type: " + parts[0]);
            }

            const_index.emplace(name, const_defs.size());
            const_defs.push_back(ConstDef {name, value});
            continue;
        }

        if (section == Section::Chunk) {
            if (current_chunk == nullptr) {
                throw AsmError(line_no, "internal parser error: null current chunk");
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

    std::unordered_map<std::string, std::size_t> chunk_index;
    chunk_index.reserve(chunk_defs.size());
    for (std::size_t i = 0; i < chunk_defs.size(); ++i) {
        chunk_index.emplace(chunk_defs[i].name, i);
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

    cu->opcodes_.clear();
    cu->chunks_.clear();
    cu->chunks_.reserve(chunk_defs.size());

    for (ChunkDef& chunk : chunk_defs) {
        // First pass: compute label offsets.
        std::size_t local_offset = 0;
        std::unordered_map<std::string, std::size_t> label_offsets;
        for (const InstDef& inst : chunk.insts) {
            if (inst.op == "__LABEL__") {
                label_offsets[inst.args[0]] = local_offset;
                continue;
            }
            local_offset += inst_size(inst, const_index, chunk_index);
        }

        const std::size_t code_begin = cu->opcodes_.size();
        local_offset = 0;
        for (const InstDef& inst : chunk.insts) {
            if (inst.op == "__LABEL__") {
                continue;
            }
            emit_inst(cu->opcodes_, inst, local_offset, label_offsets, const_index, chunk_index);
            local_offset += inst_size(inst, const_index, chunk_index);
        }
        const std::size_t code_end = cu->opcodes_.size();
        cu->chunks_.push_back(suru::vm::Chunk {
            chunk.name,
            code_begin,
            code_end,
            chunk.arity,
            chunk.slots,
            chunk.upvalues,
        });
    }

    const std::size_t main_index = chunk_index.at("main");
    const std::size_t main_upvalues = cu->chunks_[main_index].upvalues;
    suru::vm::Closure* entry = vm.make_closure(cu, main_index, main_upvalues);
    vm.push_value(suru::vm::Value::closure(entry));
    vm.call(0, 0);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h") {
            print_help(std::cout);
            return argc == 2 ? 0 : 1;
        }

        return run_file(argv[1]);
    } catch (const AsmError& e) {
        if (e.line > 0) {
            std::cerr << argv[1] << ':' << e.line << ": error: " << e.what() << '\n';
        } else {
            std::cerr << "error: " << e.what() << '\n';
        }
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "runtime error: " << e.what() << '\n';
        return 1;
    }
}
