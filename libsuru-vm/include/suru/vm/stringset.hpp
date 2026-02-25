#pragma once

#include <string_view>
#include <unordered_set>

#include "suru/vm/object.hpp"

namespace suru::vm {

struct StringSetHash {
    using is_transparent = void;

    std::size_t operator()(const String* value) const noexcept {
        return value->hash;
    }
    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view> {}(value);
    }
};

struct StringSetEq {
    using is_transparent = void;

    bool operator()(const String* lhs, const String* rhs) const noexcept {
        return lhs->view() == rhs->view();
    }
    bool operator()(const String* lhs, std::string_view rhs) const noexcept {
        return lhs->view() == rhs;
    }
    bool operator()(std::string_view lhs, const String* rhs) const noexcept {
        return lhs == rhs->view();
    }
};

using StringSet = std::unordered_set<String*, StringSetHash, StringSetEq>;

} // namespace suru::vm
