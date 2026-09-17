#include "suru/vm/object.hpp"

#include <cmath>

namespace suru::vm {
namespace {

bool validate_table_key(Value key) {
    return key.kind != ValueKind::Nil
        && !(key.kind == ValueKind::Number && std::isnan(key.number_));
}

} // namespace

bool Table::set(Value key, Value value) {
    if (!validate_table_key(key)) {
        return false;
    }
    entries[key] = value;
    return true;
}

bool Table::get(Value key, Value* out) const {
    if (!validate_table_key(key) || out == nullptr) {
        return false;
    }

    auto it = entries.find(key);
    if (it == entries.end()) {
        return false;
    }

    *out = it->second;
    return true;
}

bool Table::has(Value key) const {
    if (!validate_table_key(key)) {
        return false;
    }
    return entries.find(key) != entries.end();
}

bool Table::erase(Value key) {
    if (!validate_table_key(key)) {
        return false;
    }
    return entries.erase(key) > 0U;
}

} // namespace suru::vm
