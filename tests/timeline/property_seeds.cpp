#include "property_run.hpp"

#include <cstdlib>

namespace property {

namespace {

std::uint64_t setting(const char* name, std::uint64_t fallback) {
    const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read on the test thread
    return value ? std::strtoull(value, nullptr, 10) : fallback;
}

} // namespace

Totals run_seeds() {
    const std::uint64_t only = setting("OMA_PROPERTY_SEED", 0);
    const std::uint64_t runs = only ? 1 : setting("OMA_PROPERTY_RUNS", 48);
    const auto steps = static_cast<int>(setting("OMA_PROPERTY_STEPS", 60));
    Totals totals;
    totals.standard = only == 0 && runs == 48 && steps == 60;
    for (std::uint64_t i = 0; i < runs; ++i) {
        const std::uint64_t seed = only ? only : i + 1;
        const Run run = run_seed(seed, steps);
        totals.transitions += run.transitions;
        totals.interactions.insert(run.interactions.begin(), run.interactions.end());
        if (run.failure)
            totals.failures += report(seed, run);
    }
    return totals;
}

} // namespace property
