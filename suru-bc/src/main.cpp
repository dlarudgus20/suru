#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "suru/ir/assembler.hpp"
#include "suru/ir/disassembler.hpp"
#include "suru/ir/error.hpp"
#include "suru/ir/image.hpp"
#include "suru/lib/lib.hpp"
#include "suru/vm/error.hpp"
#include "suru/vm/vm.hpp"

namespace {

struct Options {
    std::filesystem::path input;
    std::optional<std::filesystem::path> output;
    bool disassemble {false};
};

void help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru-bc input.sura\n"
        << "  suru-bc input.sura -o output.sbc\n"
        << "  suru-bc input.sbc\n"
        << "  suru-bc --disassemble input.sbc [-o output.sura]\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    bool has_input = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") { help(std::cout); std::exit(0); }
        if (arg == "--disassemble" || arg == "-d") options.disassemble = true;
        else if (arg == "-o") {
            if (++i == argc) throw std::runtime_error("-o requires a path");
            options.output = argv[i];
        } else if (!arg.empty() && arg.front() == '-') throw std::runtime_error("unknown option: " + std::string(arg));
        else {
            if (has_input) throw std::runtime_error("only one input may be specified");
            options.input = argv[i]; has_input = true;
        }
    }
    if (!has_input) throw std::runtime_error("missing input");
    return options;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("failed to open input: " + path.string());
    std::ostringstream out; out << file.rdbuf(); return out.str();
}

suru::ir::CodeUnit read_binary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("failed to open input: " + path.string());
    return suru::ir::read_binary(file);
}

void write_binary(const std::filesystem::path& path, const suru::ir::CodeUnit& unit) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("failed to open output: " + path.string());
    suru::ir::write_binary(file, unit);
}

void disassemble(const std::optional<std::filesystem::path>& path, const suru::ir::CodeUnit& unit) {
    if (!path) { suru::ir::disassemble(std::cout, unit); return; }
    std::ofstream file(*path);
    if (!file) throw std::runtime_error("failed to open output: " + path->string());
    suru::ir::disassemble(file, unit);
}

void execute(const suru::ir::CodeUnit& unit) {
    suru::vm::VM vm; suru::lib::load_libs(vm);
    auto* closure = vm.load_code_unit(unit);
    vm.push_value(suru::vm::Value::closure(closure));
    vm.call(0, 0);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const bool binary = options.input.extension() == ".sbc";
        suru::ir::CodeUnit unit = binary
            ? read_binary(options.input) : suru::ir::assemble(read_text(options.input));
        if (options.disassemble) { disassemble(options.output, unit); return 0; }
        if (options.output) {
            if (binary) throw std::runtime_error("binary input with -o requires --disassemble");
            write_binary(*options.output, unit); return 0;
        }
        execute(unit); return 0;
    } catch (const suru::ir::AssemblerError& error) {
        std::cerr << "assembly error at line " << error.line() << ": " << error.what() << '\n'; return 1;
    } catch (const suru::vm::RuntimeError& error) {
        std::cerr << "runtime error: " << error.what() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n'; help(std::cerr); return 1;
    }
}
