#pragma once

// Applies property steps to the shared fixture and checks each one
// (Docs/implementation-plan-qa-timeline.md). It keeps the expected history: the canonical
// states an undo must return to and the ones a redo must bring back.

#include "fixture.hpp"
#include "property_ops.hpp"
#include "property_state.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace property {

struct Failure {
    std::string kind; // stable across shrinking: what kind of property broke
    std::string what; // the details, with this run's IDs
};

class Checker {
public:
    Checker();

    [[nodiscard]] const oma::timeline::Timeline& timeline() const noexcept;
    // Applies one step; the first property it breaks.
    [[nodiscard]] std::optional<Failure> step(const Op& op);
    // Undoes every edit back to the first state, then redoes them all, checking each state.
    [[nodiscard]] std::optional<Failure> unwind();
    // Successful edits, undos and redos so far (useful transitions, not steps tried).
    [[nodiscard]] int transitions() const noexcept;
    // Pairs of consecutive successful step kinds (command interactions covered).
    [[nodiscard]] const std::set<std::pair<Kind, Kind>>& interactions() const noexcept;

private:
    [[nodiscard]] std::optional<Failure> rejected(const Op& op, const State& before,
                                                  std::uint64_t revision, bool undo, bool redo);
    [[nodiscard]] std::optional<Failure> accepted(const Op& op, const State& after);

    timeline_test::Fixture fx_;
    std::vector<State> past_;   // S0 … current
    std::vector<State> future_; // states a redo brings back, the next one last
    std::uint64_t largest_id_ = 0;
    int transitions_ = 0;
    std::set<std::pair<Kind, Kind>> pairs_;
    std::optional<Kind> last_;
};

} // namespace property
