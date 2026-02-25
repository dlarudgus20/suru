#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

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

    static constexpr Value nil() {
        return Value {};
    }

    static constexpr Value boolean(bool value) {
        Value out;
        out.kind = ValueKind::Boolean;
        out.bool_ = value;
        return out;
    }

    static constexpr Value number(double value) {
        Value out;
        out.kind = ValueKind::Number;
        out.number_ = value;
        return out;
    }

    static constexpr Value string(String* value) {
        Value out;
        out.kind = ValueKind::String;
        out.string_ = value;
        return out;
    }

    static constexpr Value table(Table* value) {
        Value out;
        out.kind = ValueKind::Table;
        out.table_ = value;
        return out;
    }

    static constexpr Value closure(Closure* value) {
        Value out;
        out.kind = ValueKind::Closure;
        out.closure_ = value;
        return out;
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

