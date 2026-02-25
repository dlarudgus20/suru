#include "suru/vm/vm.hpp"

#include <functional>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <stdexcept>

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
    global_table_ = load_table();
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

String* VM::load_string(std::string_view text) {
    auto it = interned_strings_.find(text);
    if (it != interned_strings_.end()) {
        return *it;
    }

    String* object = allocate_object<String>(text.size(), 1);
    std::memcpy(const_cast<char*>(object->data()), text.data(), text.size());
    object->len = text.size();
    object->hash = std::hash<std::string_view> {}(text);
    interned_strings_.emplace(object);

    v_stack_.push_back(Value::string(object));
    return object;
}

Table* VM::load_table() {
    Table* object = allocate_object<Table>(0, 1);
    v_stack_.push_back(Value::table(object));
    return object;
}

Closure* VM::load_closure_c(CFunction func, size_t n) {
    Closure* object = allocate_object<Closure>(n * sizeof(Value), alignof(Value));
    object->code = nullptr;
    object->cfunc = func;
    object->len = n;
    for (std::size_t i = 0; i < n; ++i) {
        new (&object->at(i)) Value {};
    }
    v_stack_.push_back(Value::closure(object));
    return object;
}

CodeUnit* VM::load_code_unit() {
    code_units_.push_back(std::make_unique<CodeUnit>());
    return code_units_.back().get();
}

Closure* VM::load_closure(CodeUnit* cu, size_t chunk_index, size_t n) {
    Closure* object = allocate_object<Closure>(n * sizeof(Value), alignof(Value));
    object->code = cu;
    object->chunk_index = chunk_index;
    object->len = n;
    for (std::size_t i = 0; i < n; ++i) {
        new (&object->at(i)) Value {};
    }
    v_stack_.push_back(Value::closure(object));
    return object;
}

Table* VM::globals() {
    return global_table_;
}

const Table* VM::globals() const {
    return global_table_;
}

} // namespace suru::vm

