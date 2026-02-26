#pragma once

#include <memory>
#include <stdexcept>

#include "suru/vm/value.hpp"
#include "suru/vm/codeunit.hpp"

namespace suru::vm {

class VM;
struct Closure;
using CFunction = void (*)(VM* vm);

enum class ObjectKind {
    String,
    Table,
    Closure,
};

struct Object {
    ObjectKind kind;
    bool marked {false};
    Object* next {nullptr};

    constexpr explicit Object(ObjectKind kind) : kind(kind) {}
};

struct String : Object {
    std::size_t len {0};
    std::size_t hash {0};

    constexpr String() : Object(ObjectKind::String) {}

    const char* data() const {
        return reinterpret_cast<const char*>(this) + sizeof(*this);
    }
    std::string_view view() const {
        return std::string_view {data(), len};
    }
};

struct Table : Object {
    std::unordered_map<Value, Value, ValueHash, ValueEq> entries;

    Table() : Object(ObjectKind::Table) {}

    bool set(Value key, Value value);
    bool get(Value key, Value* out) const;
    bool has(Value key) const;
    bool erase(Value key);
};

struct Closure : Object {
    CodeUnit* code {nullptr};
    union {
        CFunction cfunc {nullptr};
        std::uint32_t chunk_index;
    };

    std::size_t len {0};

    constexpr Closure() : Object(ObjectKind::Closure) {}

    ~Closure() {
        for (std::size_t i = 0; i < len; ++i) {
            at(i).~Value();
        }
    }

    Value& at(std::size_t idx) {
        if (idx >= len) {
            throw std::out_of_range("idx");
        }
        size_t header = (sizeof(*this) + alignof(Value) - 1) & ~(alignof(Value) - 1);
        char* ptr = reinterpret_cast<char*>(this) + header;
        return reinterpret_cast<Value*>(ptr)[idx];
    }
};

} // namespace suru::vm
