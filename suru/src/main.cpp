#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "suru/front/compiler.hpp"
#include "suru/vm/vm.hpp"

namespace {

void print_help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru run <file.suru>\n"
        << "  suru compile <file.suru> -o <out.bc>\n"
        << "  suru disasm <file.bc>\n";
}

std::string read_text_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "failed to open file: " + path.string();
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

int run_source(const std::filesystem::path& source_path) {
    std::string error;
    std::string source = read_text_file(source_path, error);
    if (!error.empty()) {
        std::cerr << error << '\n';
        return 1;
    }

    auto compiled = suru::front::compile(source);
    if (!compiled.ok()) {
        for (const auto& diag : compiled.diagnostics) {
            std::cerr << source_path.string() << ':' << diag.location.line << ':' << diag.location.column
                      << ": error: " << diag.message << '\n';
        }
        return 1;
    }

    auto exec = suru::vm::execute(compiled.module);
    if (exec.exit_code != 0) {
        std::cerr << "runtime error: " << exec.error_message << '\n';
        return exec.exit_code;
    }

    if (!exec.final_stack.empty()) {
        std::cout << "result: " << exec.final_stack.back() << '\n';
    }
    return 0;
}

int compile_source(const std::filesystem::path& source_path, const std::filesystem::path& output_path) {
    std::string error;
    std::string source = read_text_file(source_path, error);
    if (!error.empty()) {
        std::cerr << error << '\n';
        return 1;
    }

    auto compiled = suru::front::compile(source);
    if (!compiled.ok()) {
        for (const auto& diag : compiled.diagnostics) {
            std::cerr << source_path.string() << ':' << diag.location.line << ':' << diag.location.column
                      << ": error: " << diag.message << '\n';
        }
        return 1;
    }

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "failed to open output file: " << output_path.string() << '\n';
        return 1;
    }

    if (!suru::vm::serialize_module(compiled.module, output, &error)) {
        std::cerr << "failed to write bytecode: " << error << '\n';
        return 1;
    }

    return 0;
}

int disasm_file(const std::filesystem::path& bytecode_path) {
    std::ifstream input(bytecode_path, std::ios::binary);
    if (!input) {
        std::cerr << "failed to open bytecode file: " << bytecode_path.string() << '\n';
        return 1;
    }

    std::string error;
    auto module = suru::vm::deserialize_module(input, &error);
    if (!module) {
        std::cerr << "failed to read bytecode: " << error << '\n';
        return 1;
    }

    std::cout << suru::vm::disassemble(*module);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help(std::cerr);
        return 1;
    }

    const std::string_view command = argv[1];

    if (command == "--help" || command == "-h") {
        print_help(std::cout);
        return 0;
    }

    if (command == "run") {
        if (argc != 3) {
            print_help(std::cerr);
            return 1;
        }
        return run_source(argv[2]);
    }

    if (command == "compile") {
        if (argc != 5 || std::string_view(argv[3]) != "-o") {
            print_help(std::cerr);
            return 1;
        }
        return compile_source(argv[2], argv[4]);
    }

    if (command == "disasm") {
        if (argc != 3) {
            print_help(std::cerr);
            return 1;
        }
        return disasm_file(argv[2]);
    }

    std::cerr << "unknown command: " << command << '\n';
    print_help(std::cerr);
    return 1;
}
