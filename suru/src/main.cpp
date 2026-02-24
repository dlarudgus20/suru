#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "suru/front/dump.hpp"
#include "suru/front/parse.hpp"

namespace {

void print_help(std::ostream& out) {
    out << "Usage:\n"
        << "  suru            # start REPL\n"
        << "  suru <file>     # parse file and print syntax tree\n"
        << "  suru --help\n";
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

int parse_source(const std::filesystem::path& source_path) {
    std::string error;
    std::string source = read_text_file(source_path, error);
    if (!error.empty()) {
        std::cerr << error << '\n';
        return 1;
    }

    auto parsed = suru::front::parse(source);
    if (!parsed.ok()) {
        for (const auto& diag : parsed.diagnostics) {
            std::cerr << source_path.string() << ':' << diag.location.line << ':' << diag.location.column
                      << ": error: " << diag.message << '\n';
        }
        return 1;
    }

    std::cout << suru::front::dump(parsed.tree);
    return 0;
}

int repl() {
    suru::front::ParserSession session;
    std::string line;
    bool continuation = false;

    while (true) {
        std::cout << (continuation ? ">> " : "> ");
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            return 0;
        }

        if (!continuation && (line == ".exit" || line == ".quit")) {
            return 0;
        }

        suru::front::ParseSessionResult parsed = session.parse_fragment(line + "\n");
        if (parsed.status == suru::front::ParseStatus::Incomplete) {
            continuation = true;
            continue;
        }

        if (parsed.status == suru::front::ParseStatus::Error) {
            for (const auto& diag : parsed.diagnostics) {
                std::cerr << "<repl>:" << diag.location.line << ':' << diag.location.column
                          << ": error: " << diag.message << '\n';
            }
            session.reset();
            continuation = false;
            continue;
        }

        std::cout << suru::front::dump(parsed.tree);
        continuation = false;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        return repl();
    }

    const std::string_view arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h") {
        print_help(std::cout);
        return 0;
    }

    if (argc == 2) {
        return parse_source(argv[1]);
    }

    std::cerr << "invalid arguments\n";
    print_help(std::cerr);
    return 1;
}
