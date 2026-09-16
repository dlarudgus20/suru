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
            case ObjectKind::Array: static_cast<Array*>(cursor)->~Array(); break;
            case ObjectKind::Table: static_cast<Table*>(cursor)->~Table(); break;
            case ObjectKind::Closure: static_cast<Closure*>(cursor)->~Closure(); break;
            case ObjectKind::Upvalue: static_cast<Upvalue*>(cursor)->~Upvalue(); break;
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

Array* VM::make_array(std::size_t len) {
    Array* object = allocate_object<Array>(0, 1);
    object->elements.resize(len, Value::nil());
    return object;
}

Table* VM::make_table() {
    Table* object = allocate_object<Table>(0, 1);
    return object;
}

Closure* VM::make_closure_c(CFunction func, std::uint8_t n) {
    Closure* object = allocate_object<Closure>(n * sizeof(Upvalue*), alignof(Upvalue*));
    object->code = nullptr;
    object->cfunc = func;
    object->len = n;
    for (std::size_t i = 0; i < n; ++i) {
        Upvalue* upvalue = make_upvalue();
        upvalue->closed = Value::nil();
        upvalue->is_open = false;
        object->at(i) = upvalue;
    }
    return object;
}

CodeUnit* VM::make_code_unit() {
    code_units_.push_back(std::make_unique<CodeUnit>());
    return code_units_.back().get();
}

Closure* VM::make_closure(CodeUnit* cu, std::uint32_t chunk_index) {
    if (cu == nullptr || chunk_index >= cu->chunks_.size()) {
        throw InvalidImageError("closure chunk index out of bounds");
    }
    const Chunk& chunk = cu->chunks_[chunk_index];
    const std::size_t n = chunk.upvalue_infos.size();
    if (n > std::numeric_limits<std::uint8_t>::max()) {
        throw InvalidImageError("too many upvalues");
    }

    Closure* object = allocate_object<Closure>(n * sizeof(Upvalue*), alignof(Upvalue*));
    object->code = cu;
    object->chunk_index = chunk_index;
    object->len = static_cast<std::uint8_t>(n);
    for (std::size_t i = 0; i < n; ++i) {
        object->at(i) = nullptr;
    }
    return object;
}

Upvalue* VM::make_upvalue() {
    Upvalue* upvalue = allocate_object<Upvalue>(0, 1);
    upvalue->closed = Value::nil();
    upvalue->slot = 0;
    upvalue->is_open = false;
    upvalue->next_open = nullptr;
    return upvalue;
}

Upvalue* VM::capture_upvalue(std::uint32_t abs_slot) {
    if (abs_slot >= v_stack_.size()) {
        throw InvalidCodeError("upvalue capture slot out of bounds");
    }

    Upvalue* prev = nullptr;
    Upvalue* curr = open_upvalues_;

    while (curr != nullptr && curr->slot > abs_slot) {
        prev = curr;
        curr = curr->next_open;
    }
    if (curr != nullptr && curr->slot == abs_slot) {
        return curr;
    }

    Upvalue* created = make_upvalue();
    created->slot = abs_slot;
    created->is_open = true;
    created->next_open = curr;
    if (prev == nullptr) {
        open_upvalues_ = created;
    } else {
        prev->next_open = created;
    }
    return created;
}

void VM::close_upvalues(std::uint32_t from_base) {
    if (from_base > v_stack_.size()) {
        throw InternalError("close upvalues base out of bounds");
    }
    if (from_base == v_stack_.size()) {
        return;
    }

    while (open_upvalues_ != nullptr && open_upvalues_->slot >= from_base) {
        Upvalue* upvalue = open_upvalues_;
        if (upvalue->slot >= v_stack_.size()) {
            throw InternalError("open upvalue slot out of bounds");
        }
        upvalue->closed = v_stack_[upvalue->slot];
        upvalue->is_open = false;
        open_upvalues_ = upvalue->next_open;
        upvalue->next_open = nullptr;
    }
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
    const auto begin = i_stack_.size() == 1 ? 0U : i_stack_.back().base + 1U;
    if (v_stack_.size() <= begin) {
        throw InternalError("frame stack underflow");
    }
    Value out = v_stack_.back();
    close_upvalues(static_cast<std::uint32_t>(v_stack_.size() - 1U));
    v_stack_.pop_back();
    i_stack_.back().top = static_cast<std::uint32_t>(v_stack_.size());
    return out;
}

void VM::push_value(Value value) {
    if (v_stack_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw StackOverflowError("stack exceeds uint32 range");
    }
    v_stack_.push_back(value);
    i_stack_.back().top = static_cast<std::uint32_t>(v_stack_.size());
}

std::size_t VM::stack_top() const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    const std::size_t base = i_stack_.size() == 1 ? 0U : i_stack_.back().base + 1U;
    if (v_stack_.size() < base) {
        throw InternalError("frame base is out of stack bounds");
    }
    return v_stack_.size() - base;
}

Value VM::getlocal(std::uint32_t index) const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    const std::size_t begin = i_stack_.size() == 1 ? 0U : i_stack_.back().base + 1U;
    const std::size_t at = begin + index;
    if (at >= v_stack_.size()) {
        throw ApiError("local index out of bounds");
    }
    return v_stack_[at];
}

Value VM::getupvalue(std::uint8_t index) const {
    if (i_stack_.empty()) {
        throw InternalError("call frame is not available");
    }
    Closure* closure = frame_closure();
    if (closure == nullptr) {
        // the number of upvalues of sentinel callframe is treated as 0
        throw ApiError("upvalue index out of bounds");
    }
    if (index >= closure->len) {
        throw ApiError("upvalue index out of bounds");
    }
    Upvalue* upvalue = closure->at(index);
    if (upvalue == nullptr) {
        throw InternalError("upvalue is not initialized");
    }
    if (!upvalue->is_open) {
        return upvalue->closed;
    }
    if (upvalue->slot >= v_stack_.size()) {
        throw InternalError("open upvalue slot out of bounds");
    }
    return v_stack_[upvalue->slot];
}

} // namespace suru::vm
