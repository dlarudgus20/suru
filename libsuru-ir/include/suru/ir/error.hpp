#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace suru::ir {

class AssemblerError : public std::runtime_error {
public:
    AssemblerError(std::size_t line, std::string message)
        : std::runtime_error(std::move(message)), line_(line) {}

    [[nodiscard]] std::size_t line() const noexcept { return line_; }

private:
    std::size_t line_;
};

class ImageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace suru::ir
