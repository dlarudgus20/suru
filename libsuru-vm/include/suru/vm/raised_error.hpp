#pragma once

#include <string>
#include <utility>

#include "suru/vm/error.hpp"
#include "suru/vm/value.hpp"

namespace suru::vm {

class RaisedError final : public RuntimeError {
public:
    RaisedError(Value payload, std::string display)
        : RuntimeError(RuntimeErrorCategory::Raised, std::move(display)), payload_(payload) {}

    [[nodiscard]] Value payload() const noexcept {
        return payload_;
    }

private:
    Value payload_;
};

} // namespace suru::vm
