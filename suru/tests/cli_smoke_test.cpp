#include <cstdlib>

int main() {
#if defined(_WIN32)
    const int rc = std::system("suru --help > NUL");
#else
    const int rc = std::system("./suru --help > /dev/null");
#endif
    return rc == 0 ? 0 : 1;
}
