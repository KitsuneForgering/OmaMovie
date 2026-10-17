#include "property_checker.hpp"

namespace property {

Checker::Checker() : fx_(timeline_test::make_fixture()) {
    fx_.editor.clear_history(); // S0 is the fixture: the first undo is the first step
    past_.push_back(canonical(fx_.editor.timeline()));
    for (const auto& track : fx_.editor.timeline().tracks())
        largest_id_ = std::max(largest_id_, track.id.value());
}

const oma::timeline::Timeline& Checker::timeline() const noexcept {
    return fx_.editor.timeline();
}

int Checker::transitions() const noexcept {
    return transitions_;
}

const std::set<std::pair<Kind, Kind>>& Checker::interactions() const noexcept {
    return pairs_;
}

std::optional<Failure> Checker::step(const Op& op) {
    auto& ed = fx_.editor;
    const State before = canonical(ed.timeline());
    const std::uint64_t revision = ed.revision();
    const bool undo = ed.can_undo();
    const bool redo = ed.can_redo();
    oma::Result<void> result;
    if (op.kind == Kind::Undo)
        result = ed.undo();
    else if (op.kind == Kind::Redo)
        result = ed.redo();
    else
        result = ed.execute(to_command(op, ed));
    // An edit that changes nothing succeeds without a history entry: checked like a rejection.
    const bool history = op.kind == Kind::Undo || op.kind == Kind::Redo;
    if (!result || (!history && ed.revision() == revision))
        return rejected(op, before, revision, undo, redo);
    if (ed.revision() <= revision)
        return Failure{"revision", "a successful step did not advance the revision"};
    ++transitions_;
    if (last_)
        pairs_.emplace(*last_, op.kind);
    last_ = op.kind;
    return accepted(op, canonical(ed.timeline()));
}

} // namespace property
