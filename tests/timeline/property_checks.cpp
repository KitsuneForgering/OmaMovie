#include "property_checker.hpp"
#include "property_invariants.hpp"

#include <format>

namespace property {

std::optional<Failure> Checker::rejected(const Op& op, const State& before, std::uint64_t revision,
                                         bool undo, bool redo) {
    const auto& ed = fx_.editor;
    if (auto diff = difference(before, canonical(ed.timeline())); !diff.empty())
        return Failure{"rejected-changed", "a rejected step changed the timeline: " + diff};
    if (ed.revision() != revision || ed.can_undo() != undo || ed.can_redo() != redo)
        return Failure{"rejected-history", "a rejected step changed the revision or history"};
    if ((op.kind == Kind::Undo && past_.size() > 1) || (op.kind == Kind::Redo && !future_.empty()))
        return Failure{"history-refused", "undo or redo refused with history left"};
    return std::nullopt;
}

std::optional<Failure> Checker::accepted(const Op& op, const State& after) {
    const bool history = op.kind == Kind::Undo || op.kind == Kind::Redo;
    if (op.kind == Kind::Undo) {
        if (past_.size() < 2)
            return Failure{"history-extra", "undo succeeded with nothing to undo"};
        future_.push_back(past_.back());
        past_.pop_back();
    } else if (op.kind == Kind::Redo) {
        if (future_.empty())
            return Failure{"history-extra", "redo succeeded with nothing to redo"};
        past_.push_back(future_.back());
        future_.pop_back();
    }
    if (history) {
        if (auto diff = difference(past_.back(), after); !diff.empty())
            return Failure{"history-state",
                           "undo or redo did not restore the saved state: " + diff};
        return std::nullopt;
    }
    if (after == past_.back()) // an Undo entry that does nothing
        return Failure{"no-op", "an accepted edit left the timeline unchanged: " + to_string(op)};
    if (auto broken = broken_structure(fx_.editor.timeline()); !broken.empty())
        return Failure{"invariant", broken};
    std::uint64_t largest = largest_id_;
    for (const auto& track : fx_.editor.timeline().tracks())
        for (const auto& clip : track.clips)
            if (past_.back().text.find(std::format("clip {} ", clip.id.value())) ==
                std::string::npos) {
                if (clip.id.value() <= largest_id_)
                    return Failure{"id-reuse",
                                   std::format("new clip {} reuses an ID", clip.id.value())};
                largest = std::max(largest, clip.id.value());
            }
    largest_id_ = largest;
    past_.push_back(after);
    future_.clear();
    return std::nullopt;
}

} // namespace property
