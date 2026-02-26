#include "suru/vm/vm.hpp"

#include <functional>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>

#include "suru/vm/error.hpp"

namespace suru::vm {

template <typename T>
T* VM::allocate_object(size_t size, size_t align) {
    size_t header = (sizeof(T) + align - 1) & ~(align - 1);

    T* object = static_cast<T*>(std::malloc(header + size));
    new (object) T {};

    object->next = objects_;
    objects_ = object;
    return object;
}

VM::VM() {
    global_table_ = make_table();
    i_stack_.push_back(CallFrame {});
}

VM::~VM() {
    Object* cursor = objects_;
    while (cursor != nullptr) {
        Object* next = cursor->next;
        switch (cursor->kind) {
            case ObjectKind::String: static_cast<String*>(cursor)->~String(); break;
            case ObjectKind::Table: static_cast<Table*>(cursor)->~Table(); break;
            case ObjectKind::Closure: static_cast<Closure*>(cursor)->~Closure(); break;
            default: std::unreachable();
        }
        std::free(cursor);
        cursor = next;
    }
}

String* VM::make_string(std::string_view text) {
    auto it = interned_strings_.find(text);
    if (it != interned_strings_.end()) {
        return *it;
    }

    String* object = allocate_object<String>(text.size(), 1);
    std::memcpy(const_cast<char*>(object->data()), text.data(), text.size());
    object->len = text.size();
    object->hash = std::hash<std::string_view> {}(text);
    interned_strings_.emplace(object);
    return object;
}

Table* VM::make_table() {
    Table* object = allocate_object<Table>(0, 1);
    return object;
}

Closure* VM::make_closure_c(CFunction func, std::uint8_t n) {
    Closure* object = allocate_object<Closure>(n * sizeof(Value), alignof(Value));
    object->code = nullptr;
    object->cfunc = func;
    object->len = n;
    for (std::size_t i = 0; i < n; ++i) {
        new (&object->at(i)) Value {};
    }
    return object;
}

CodeUnit* VM::make_code_unit() {
    code_units_.push_back(std::make_unique<CodeUnit>());
    return code_units_.back().get();
}

Closure* VM::make_closure(CodeUnit* cu, std::uint32_t chunk_index, std::uint8_t n) {
    Closure* object = allocate_object<Closure>(n * sizeof(Value), alignof(Value));
    object->code = cu;
    object->chunk_index = chunk_index;
    object->len = n;
    for (std::size_t i = 0; i < n; ++i) {
        new (&object->at(i)) Value {};
    }
    return object;
}

Table* VM::globals() {
    return global_table_;
}

Value VM::pop_value() {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    if (v_stack_.empty()) {
        throw ApiError("stack underflow");
    }
    if (v_stack_.size() <= i_stack_.back().base) {
        throw InternalError("frame stack underflow");
    }
    Value out = v_stack_.back();
    v_stack_.pop_back();
    return out;
}

void VM::push_value(Value value) {
    if (v_stack_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw InternalError("stack exceeds uint32 range");
    }
    v_stack_.push_back(value);
}

std::size_t VM::stack_top() const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    const std::size_t base = i_stack_.back().base;
    if (v_stack_.size() < base) {
        throw InternalError("frame base is out of stack bounds");
    }
    return v_stack_.size() - base;
}

Value VM::getlocal(std::uint8_t index) const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    const std::size_t at = i_stack_.back().base + index;
    if (at >= v_stack_.size()) {
        throw ApiError("local index out of bounds");
    }
    return v_stack_[at];
}

Value VM::getupvalue(std::uint8_t index) const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    Closure* closure = i_stack_.back().closure;
    if (closure == nullptr) {
        // the number of upvalues of sentinel callframe is treated as 0
        throw ApiError("upvalue index out of bounds");
    }
    if (index >= closure->len) {
        throw ApiError("upvalue index out of bounds");
    }
    return closure->at(index);
}

} // namespace suru::vm

