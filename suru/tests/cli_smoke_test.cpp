#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "missing suru executable path\n";
        return 1;
    }

#if defined(_WIN32)
    const std::string cmd = "\"" + std::string(argv[1]) + "\" --help > NUL";
#else
    const std::string cmd = "\"" + std::string(argv[1]) + "\" --help > /dev/null";
#endif

    const int rc = std::system(cmd.c_str());
    return rc == 0 ? 0 : 1;
}
