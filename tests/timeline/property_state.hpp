#pragma once

// The test-only canonical state of a timeline (docs/implementation-plan-qa-timeline.md): every
// persisted edit field, exact times with their timebases, and nothing transient. Independent of
// the project format, which round trips are checked against separately.

#include "oma/timeline/model.hpp"

#include <string>
#include <vector>

namespace property {

struct State {
    // One line per track, clip, marker and caption, in timeline order.
    std::string text;
    // Per clip in the same order: properties compared whole rather than printed.
    std::vector<oma::timeline::VideoProperties> video;
    std::vector<oma::timeline::AudioProperties> audio;

    friend bool operator==(const State&, const State&) = default;
};

[[nodiscard]] State canonical(const oma::timeline::Timeline& tl);
// Empty when equal; otherwise the first line that differs in each, for the failure report.
[[nodiscard]] std::string difference(const State& expected, const State& actual);

} // namespace property
