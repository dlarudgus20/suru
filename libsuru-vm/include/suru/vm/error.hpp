#pragma once

#include <exception>
#include <string>
#include <utility>

namespace suru::vm {

enum class RuntimeErrorCategory {
    Raised,
    Type,
    Api,
    Table,
    InvalidCode,
    InvalidImage,
    StackOverflow,
    Internal,
};

class RuntimeError : public std::exception {
public:
    RuntimeError(RuntimeErrorCategory category, std::string message)
        : category_(category), message_(std::move(message)) {}

    [[nodiscard]] RuntimeErrorCategory category() const noexcept {
        return category_;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    RuntimeErrorCategory category_;
    std::string message_;
};

class TypeError : public RuntimeError {
public:
    explicit TypeError(std::string message)
        : RuntimeError(RuntimeErrorCategory::Type, std::move(message)) {}
};

class ApiError : public RuntimeError {
public:
    explicit ApiError(std::string message)
        : RuntimeError(RuntimeErrorCategory::Api, std::move(message)) {}
};

class TableError : public RuntimeError {
public:
    explicit TableError(std::string message)
        : RuntimeError(RuntimeErrorCategory::Table, std::move(message)) {}
};

class InvalidCodeError : public RuntimeError {
public:
    explicit InvalidCodeError(std::string message)
        : RuntimeError(RuntimeErrorCategory::InvalidCode, std::move(message)) {}
};

class InvalidImageError : public RuntimeError {
public:
    explicit InvalidImageError(std::string message)
        : RuntimeError(RuntimeErrorCategory::InvalidImage, std::move(message)) {}
};

class StackOverflowError : public RuntimeError {
public:
    explicit StackOverflowError(std::string message)
        : RuntimeError(RuntimeErrorCategory::StackOverflow, std::move(message)) {}
};

class InternalError : public RuntimeError {
public:
    explicit InternalError(std::string message)
        : RuntimeError(RuntimeErrorCategory::Internal, std::move(message)) {}
};

} // namespace suru::vm
