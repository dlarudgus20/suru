#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "suru/vm/error.hpp"

namespace suru::vm {

enum class ValueKind {
    Nil,
    Boolean,
    Number,
    String,
    Table,
    Closure,
};

struct String;
struct Table;
struct Closure;

struct Value {
    ValueKind kind {ValueKind::Nil};
    union {
        bool bool_;
        double number_;
        String* string_;
        Table* table_;
        Closure* closure_;
    };

    constexpr Value() : kind(ValueKind::Nil), table_(nullptr) {}

    [[nodiscard]] static constexpr Value nil() {
        return Value {};
    }

    [[nodiscard]] static constexpr Value boolean(bool value) {
        Value out;
        out.kind = ValueKind::Boolean;
        out.bool_ = value;
        return out;
    }

    [[nodiscard]] static constexpr Value number(double value) {
        Value out;
        out.kind = ValueKind::Number;
        out.number_ = value;
        return out;
    }

    [[nodiscard]] static constexpr Value string(String* value) {
        Value out;
        out.kind = ValueKind::String;
        out.string_ = value;
        return out;
    }

    [[nodiscard]] static constexpr Value table(Table* value) {
        Value out;
        out.kind = ValueKind::Table;
        out.table_ = value;
        return out;
    }

    [[nodiscard]] static constexpr Value closure(Closure* value) {
        Value out;
        out.kind = ValueKind::Closure;
        out.closure_ = value;
        return out;
    }

    [[nodiscard]] constexpr bool is_nil() const {
        return kind == ValueKind::Nil;
    }

    [[nodiscard]] constexpr bool is_boolean() const {
        return kind == ValueKind::Boolean;
    }

    [[nodiscard]] constexpr bool is_number() const {
        return kind == ValueKind::Number;
    }

    [[nodiscard]] constexpr bool is_string() const {
        return kind == ValueKind::String;
    }

    [[nodiscard]] constexpr bool is_table() const {
        return kind == ValueKind::Table;
    }

    [[nodiscard]] constexpr bool is_closure() const {
        return kind == ValueKind::Closure;
    }

    [[nodiscard]] constexpr bool is_falsy() const {
        return is_nil() || (is_boolean() && !bool_);
    }

    [[nodiscard]] constexpr bool is_truthy() const {
        return !is_falsy();
    }

    [[nodiscard]] bool as_boolean(std::string_view where) const {
        if (!is_boolean()) {
            type_error(where, "boolean");
        }
        return bool_;
    }

    [[nodiscard]] double as_number(std::string_view where) const {
        if (!is_number()) {
            type_error(where, "number");
        }
        return number_;
    }

    [[nodiscard]] std::int64_t as_integer(std::string_view where) const {
        return static_cast<std::int64_t>(as_number(where));
    }

    [[nodiscard]] String* as_string(std::string_view where) const {
        if (!is_string() || string_ == nullptr) {
            type_error(where, "string");
        }
        return string_;
    }

    [[nodiscard]] Table* as_table(std::string_view where) const {
        if (!is_table() || table_ == nullptr) {
            type_error(where, "table");
        }
        return table_;
    }

    [[nodiscard]] Closure* as_closure(std::string_view where) const {
        if (!is_closure() || closure_ == nullptr) {
            type_error(where, "closure");
        }
        return closure_;
    }

private:
    [[noreturn]] static void type_error(std::string_view where, std::string_view expected) {
        throw TypeError(std::string(where) + ": expected " + std::string(expected));
    }
};

inline bool value_equals(Value lhs, Value rhs) {
    if (lhs.kind != rhs.kind) {
        return false;
    }

    switch (lhs.kind) {
        case ValueKind::Nil: return true;
        case ValueKind::Boolean: return lhs.bool_ == rhs.bool_;
        case ValueKind::Number: return lhs.number_ == rhs.number_;
        case ValueKind::String: return lhs.string_ == rhs.string_;
        case ValueKind::Table: return lhs.table_ == rhs.table_;
        case ValueKind::Closure: return lhs.closure_ == rhs.closure_;
        default: return false;
    }
}

inline std::size_t value_hash(Value value) {
    auto combine_hash = [](std::size_t seed, std::size_t v) {
        return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
    };

    std::size_t seed = static_cast<std::size_t>(value.kind);
    switch (value.kind) {
        case ValueKind::Nil: return seed;
        case ValueKind::Boolean: return combine_hash(seed, std::hash<bool> {}(value.bool_));
        case ValueKind::Number: {
            const double normalized = (value.number_ == 0.0) ? 0.0 : value.number_;
            return combine_hash(seed, std::hash<double> {}(normalized));
        }
        case ValueKind::String:
            return combine_hash(seed, std::hash<std::uintptr_t> {}(reinterpret_cast<std::uintptr_t>(value.string_)));
        case ValueKind::Table:
            return combine_hash(seed, std::hash<std::uintptr_t> {}(reinterpret_cast<std::uintptr_t>(value.table_)));
        case ValueKind::Closure:
            return combine_hash(seed, std::hash<std::uintptr_t> {}(reinterpret_cast<std::uintptr_t>(value.closure_)));
        default:
            return seed;
    }
}

struct ValueHash {
    std::size_t operator()(Value value) const {
        return value_hash(value);
    }
};

struct ValueEq {
    bool operator()(Value lhs, Value rhs) const {
        return value_equals(lhs, rhs);
    }
};

} // namespace suru::vm

