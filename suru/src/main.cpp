#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "suru/front/compile.hpp"
#include "suru/front/dump.hpp"
#include "suru/front/parse.hpp"
#include "suru/ir/disassembler.hpp"
#include "suru/ir/image.hpp"
#include "suru/lib/lib.hpp"
#include "suru/vm/error.hpp"
#include "suru/vm/raised_error.hpp"
#include "suru/vm/vm.hpp"

namespace {

enum class Mode { Execute, Ast, Ir };

struct Options {
    Mode mode {Mode::Execute};
    std::optional<std::string> input;
    std::optional<std::filesystem::path> output;
};

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru [file.suru|file.sbc|-]\n"
        << "  suru --ast [file.suru|-] [-o output.yaml]\n"
        << "  suru --ir  [file.suru|-] [-o output.sbc]\n"
        << "\nWithout a file, the selected mode starts a REPL.\n";
}

Options options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") { help(std::cout); std::exit(0); }
        if (arg == "--ast" || arg == "--ir") {
            const Mode next = arg == "--ast" ? Mode::Ast : Mode::Ir;
            if (result.mode != Mode::Execute) throw CliError("--ast and --ir are mutually exclusive");
            result.mode = next;
        } else if (arg == "-o") {
            if (++i == argc) throw CliError("-o requires a path");
            if (result.output) throw CliError("-o may only be specified once");
            result.output = argv[i];
        } else if (!arg.empty() && arg.front() == '-' && arg != "-") {
            throw CliError("unknown option: " + std::string(arg));
        } else {
            if (result.input) throw CliError("only one input may be specified");
            result.input = std::string(arg);
        }
    }
    if (result.mode == Mode::Execute && result.output) throw CliError("-o requires --ast or --ir");
    if (!result.input && result.output) throw CliError("-o is not supported in REPL mode");
    return result;
}

std::string read_source(const std::string& input) {
    if (input == "-") {
        std::ostringstream text; text << std::cin.rdbuf(); return text.str();
    }
    std::ifstream file(input);
    if (!file) throw CliError("failed to open input: " + input);
    std::ostringstream text; text << file.rdbuf(); return text.str();
}

void diagnostics(const std::vector<suru::front::Diagnostic>& values, std::string_view filename) {
    for (const auto& value : values) {
        std::cerr << filename << ':' << value.location.line << ':' << value.location.column
                  << ": error: " << value.message << '\n';
    }
}

suru::front::ParseResult parse_source(std::string_view source, std::string_view filename) {
    auto parsed = suru::front::parse(source, filename);
    if (!parsed.ok()) diagnostics(parsed.diagnostics, filename);
    return parsed;
}

suru::front::CompileResult compile_source(const suru::front::Ast& ast, std::string_view filename) {
    auto compiled = suru::front::compile(ast);
    if (!compiled.ok()) diagnostics(compiled.diagnostics, filename);
    return compiled;
}

void write_text(std::string_view value, const std::optional<std::filesystem::path>& output) {
    if (!output) { std::cout << value; return; }
    std::ofstream file(*output);
    if (!file) throw CliError("failed to open output: " + output->string());
    file << value;
    if (!file) throw CliError("failed to write output: " + output->string());
}

void write_ir(const suru::ir::CodeUnit& code, const std::optional<std::filesystem::path>& output) {
    if (!output) { suru::ir::write_binary(std::cout, code); return; }
    std::ofstream file(*output, std::ios::binary);
    if (!file) throw CliError("failed to open output: " + output->string());
    suru::ir::write_binary(file, code);
}

suru::ir::CodeUnit read_ir(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw CliError("failed to open input: " + path.string());
    return suru::ir::read_binary(file);
}

void execute(suru::vm::VM& vm, const suru::ir::CodeUnit& code) {
    suru::vm::Closure* entry = vm.load_code_unit(code);
    vm.push_value(suru::vm::Value::closure(entry));
    vm.call(0, 0);
}

int file_mode(const Options& option) {
    const std::string& input = *option.input;
    if (option.mode == Mode::Execute && input != "-" && std::filesystem::path(input).extension() == ".sbc") {
        suru::vm::VM vm; suru::lib::load_libs(vm); execute(vm, read_ir(input)); return 0;
    }
    if (input != "-" && std::filesystem::path(input).extension() == ".sbc") {
        throw CliError("--ast and --ir require source input");
    }

    const std::string source = read_source(input);
    auto parsed = parse_source(source, input == "-" ? "<stdin>" : input);
    if (!parsed.ok()) return 1;
    if (option.mode == Mode::Ast) { write_text(suru::front::dump(parsed.tree), option.output); return 0; }
    auto compiled = compile_source(parsed.tree, parsed.filename);
    if (!compiled.ok()) return 1;
    if (option.mode == Mode::Ir) { write_ir(compiled.code, option.output); return 0; }
    suru::vm::VM vm; suru::lib::load_libs(vm); execute(vm, compiled.code); return 0;
}

int repl(Mode mode) {
    suru::front::ParseContext context {"<repl>"};
    suru::vm::VM vm;
    if (mode == Mode::Execute) suru::lib::load_libs(vm);
    bool continuation = false;
    std::string line;
    while (true) {
        std::cerr << (continuation ? ">> " : "> ");
        std::cerr.flush();
        if (!std::getline(std::cin, line)) { std::cerr << '\n'; return 0; }
        if (!continuation && (line == ".quit" || line == ".exit")) return 0;
        auto parsed = suru::front::parse(line + "\n", context);
        if (parsed.status == suru::front::ParseStatus::Incomplete) { continuation = true; continue; }
        continuation = false;
        if (!parsed.ok()) { diagnostics(parsed.diagnostics, parsed.filename); continue; }
        if (mode == Mode::Ast) { std::cout << suru::front::dump(parsed.tree); continue; }
        auto compiled = compile_source(parsed.tree, parsed.filename);
        if (!compiled.ok()) continue;
        if (mode == Mode::Ir) { suru::ir::disassemble(std::cout, compiled.code); std::cout.flush(); continue; }
        try { execute(vm, compiled.code); }
        catch (const suru::vm::RuntimeError& error) { std::cerr << "runtime error: " << error.what() << '\n'; }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options option = options(argc, argv);
        return option.input ? file_mode(option) : repl(option.mode);
    } catch (const CliError& error) {
        std::cerr << "error: " << error.what() << '\n'; help(std::cerr); return 2;
    } catch (const suru::ir::ImageError& error) {
        std::cerr << "IR error: " << error.what() << '\n'; return 1;
    } catch (const suru::vm::RuntimeError& error) {
        std::cerr << "runtime error: " << error.what() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n'; return 1;
    }
}
