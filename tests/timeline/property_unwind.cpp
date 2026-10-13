#include "property_checker.hpp"

namespace property {

std::optional<Failure> Checker::unwind() {
    const std::size_t edits = past_.size() - 1;
    for (std::size_t i = 0; i < edits; ++i)
        if (auto f = step(Op{.kind = Kind::Undo}))
            return f;
    if (auto f = step(Op{.kind = Kind::Undo})) // nothing left: refused, state unchanged
        return f;
    for (std::size_t i = 0; i < edits; ++i)
        if (auto f = step(Op{.kind = Kind::Redo}))
            return f;
    return std::nullopt;
}

} // namespace property
