#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "suru/front/compile.hpp"
#include "suru/front/dump.hpp"
#include "suru/front/parse.hpp"
#include "suru/ir/assembler.hpp"
#include "suru/ir/disassembler.hpp"
#include "suru/ir/image.hpp"
#include "suru/lib/lib.hpp"
#include "suru/vm/error.hpp"
#include "suru/vm/raised_error.hpp"
#include "suru/vm/vm.hpp"

namespace {

enum class Mode { Execute, Ast, Ir, Sbc, Disas };
enum class InputFormat { Source, Ir, Sbc };

struct Options {
    Mode mode {Mode::Execute};
    InputFormat format {InputFormat::Source};
    std::optional<InputFormat> explicit_format;
    std::optional<std::string> input;
    std::optional<std::filesystem::path> output;
};

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru [--in=src|ir|sbc] [input|-]\n"
        << "  suru --ast [--in=src] [input|-] [-o output.yaml]\n"
        << "  suru --ir [--in=src|ir] [input|-] [-o output.sura]\n"
        << "  suru --sbc [--in=src|ir] input|- -o output.sbc\n"
        << "  suru --disas [--in=sbc] input [-o output.sura]\n"
        << "\nWithout --in, .sura selects IR, .sbc selects SBC, otherwise source.\n"
        << "Extensions are case-insensitive; explicit --in overrides them.\n"
        << "Without input, source execute/--ast/--ir starts a REPL.\n"
        << "SBC stdin and binary stdout are not supported.\n";
}

InputFormat inferred_format(const std::optional<std::string>& input) {
    if (!input || *input == "-") return InputFormat::Source;
    std::string extension = std::filesystem::path(*input).extension().string();
    for (char& ch : extension) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (extension == ".sura") return InputFormat::Ir;
    if (extension == ".sbc") return InputFormat::Sbc;
    return InputFormat::Source;
}

Options options(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") { help(std::cout); std::exit(0); }
        if (arg == "--ast" || arg == "--ir" || arg == "--sbc" || arg == "--disas") {
            if (result.mode != Mode::Execute) throw CliError("output modes may only be specified once and are mutually exclusive");
            if (arg == "--ast") result.mode = Mode::Ast;
            else if (arg == "--ir") result.mode = Mode::Ir;
            else if (arg == "--sbc") result.mode = Mode::Sbc;
            else result.mode = Mode::Disas;
        } else if (arg.starts_with("--in=")) {
            if (result.explicit_format) throw CliError("--in may only be specified once");
            const auto value = arg.substr(5);
            if (value == "src") result.explicit_format = InputFormat::Source;
            else if (value == "ir") result.explicit_format = InputFormat::Ir;
            else if (value == "sbc") result.explicit_format = InputFormat::Sbc;
            else throw CliError("--in requires src, ir, or sbc");
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
    result.format = result.explicit_format.value_or(inferred_format(result.input));
    if (!result.input) {
        if (result.format != InputFormat::Source || result.mode == Mode::Sbc || result.mode == Mode::Disas) {
            throw CliError("REPL supports only source execute, --ast, or --ir; this mode requires input");
        }
        if (result.output) throw CliError("-o is not supported in REPL mode");
    }
    if (result.mode == Mode::Execute && result.output) throw CliError("-o requires an output mode");
    if (result.mode == Mode::Ast && result.format != InputFormat::Source) throw CliError("--ast requires source input");
    if ((result.mode == Mode::Ir || result.mode == Mode::Sbc) && result.format == InputFormat::Sbc) {
        throw CliError("--ir and --sbc require source or IR input; use --disas for SBC");
    }
    if (result.mode == Mode::Disas && result.format != InputFormat::Sbc) throw CliError("--disas requires SBC input");
    if (result.input && *result.input == "-" && result.format == InputFormat::Sbc) throw CliError("SBC stdin is not supported");
    if (result.mode == Mode::Sbc && !result.output) throw CliError("--sbc requires -o; binary stdout is not supported");
    return result;
}

std::string read_text(const std::string& input) {
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

void write_sbc(const suru::ir::CodeUnit& code, const std::optional<std::filesystem::path>& output) {
    if (!output) throw CliError("SBC output requires -o");
    std::ofstream file(*output, std::ios::binary);
    if (!file) throw CliError("failed to open output: " + output->string());
    suru::ir::write_binary(file, code);
}

suru::ir::CodeUnit read_sbc(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw CliError("failed to open input: " + path.string());
    return suru::ir::read_binary(file);
}

void execute(suru::vm::VM& vm, const suru::ir::CodeUnit& code) {
    suru::vm::Closure* entry = vm.load_code_unit(code);
    vm.push_value(suru::vm::Value::closure(entry));
    vm.call(0, 0);
}

void write_assembly(const suru::ir::CodeUnit& code, const std::optional<std::filesystem::path>& output) {
    std::ostringstream text;
    suru::ir::disassemble(text, code);
    write_text(text.str(), output);
}

int file_mode(const Options& option) {
    const std::string& input = *option.input;
    suru::ir::CodeUnit code;
    if (option.format == InputFormat::Sbc) {
        code = read_sbc(input);
    } else {
        const std::string text = read_text(input);
        if (option.format == InputFormat::Ir) {
            code = suru::ir::assemble(text);
        } else {
            auto parsed = parse_source(text, input == "-" ? "<stdin>" : input);
            if (!parsed.ok()) return 1;
            if (option.mode == Mode::Ast) { write_text(suru::front::dump(parsed.tree), option.output); return 0; }
            auto compiled = compile_source(parsed.tree, parsed.filename);
            if (!compiled.ok()) return 1;
            code = std::move(compiled.code);
        }
    }
    if (option.mode == Mode::Ir || option.mode == Mode::Disas) { write_assembly(code, option.output); return 0; }
    if (option.mode == Mode::Sbc) { write_sbc(code, option.output); return 0; }
    suru::vm::VM vm; suru::lib::load_libs(vm); execute(vm, code); return 0;
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
    } catch (const suru::ir::AssemblerError& error) {
        std::cerr << "assembly error at line " << error.line() << ": " << error.what() << '\n'; return 1;
    } catch (const suru::ir::ImageError& error) {
        std::cerr << "IR error: " << error.what() << '\n'; return 1;
    } catch (const suru::vm::RuntimeError& error) {
        std::cerr << "runtime error: " << error.what() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n'; return 1;
    }
}
