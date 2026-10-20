#pragma once

// Seeded runs, exact replay and shrinking for the edit properties
// (Docs/implementation-plan-qa-timeline.md). A run's log replays with no random choices.

#include "property_checker.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace property {

struct Run {
    std::vector<Op> log;
    std::optional<Failure> failure;
    int transitions = 0;
    std::set<std::pair<Kind, Kind>> interactions;
};

// Generates and checks `steps` steps from `seed`, then undoes and redoes the whole history.
[[nodiscard]] Run run_seed(std::uint64_t seed, int steps);
// The same checks over a fixed log.
[[nodiscard]] Run replay(std::span<const Op> log);
// The seeds this test run asks for: 48 seeds of 60 steps by default; OMA_PROPERTY_SEED=n replays
// one seed, OMA_PROPERTY_RUNS and OMA_PROPERTY_STEPS widen the run.
struct Totals {
    std::string failures; // every failing seed's report; empty when all hold
    int transitions = 0;
    std::set<std::pair<Kind, Kind>> interactions;
    bool standard = true; // the default budget, which coverage floors are set for
};
[[nodiscard]] Totals run_seeds();
// A shorter, simpler log that still fails with the same kind of failure.
[[nodiscard]] std::vector<Op> shrink(std::vector<Op> log, const std::string& kind);
// Seed, generator version, fixture, replay command and the minimized log.
[[nodiscard]] std::string report(std::uint64_t seed, const Run& run);

// A numeric environment setting (OMA_PROPERTY_SEED, _RUNS, _STEPS), or `fallback` when unset.
[[nodiscard]] std::uint64_t setting(const char* name, std::uint64_t fallback);

} // namespace property
