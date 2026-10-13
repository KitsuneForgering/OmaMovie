#include "property_run.hpp"

#include "property_gen.hpp"

#include <format>

namespace property {

Run run_seed(std::uint64_t seed, int steps) {
    Run run;
    Checker checker;
    Rng rng(seed);
    for (int i = 0; i < steps && !run.failure; ++i) {
        run.log.push_back(generate(rng, checker.timeline()));
        run.failure = checker.step(run.log.back());
    }
    if (!run.failure)
        run.failure = checker.unwind();
    run.transitions = checker.transitions();
    run.interactions = checker.interactions();
    return run;
}

Run replay(std::span<const Op> log) {
    Run run;
    Checker checker;
    for (const Op& op : log) {
        run.log.push_back(op);
        if ((run.failure = checker.step(op)))
            break;
    }
    if (!run.failure)
        run.failure = checker.unwind();
    run.transitions = checker.transitions();
    run.interactions = checker.interactions();
    return run;
}

std::string report(std::uint64_t seed, const Run& run) {
    const std::vector<Op> small = shrink(run.log, run.failure->kind);
    std::string out =
        std::format("edit property failed: {} ({})\n  seed {} generator v{} fixture "
                    "timeline_test::make_fixture\n"
                    "  replay: OMA_PROPERTY_SEED={} build/debug/tests/timeline_tests\n"
                    "  minimized log ({} of {} steps):\n",
                    run.failure->kind, run.failure->what, seed, kGeneratorVersion, seed,
                    small.size(), run.log.size());
    for (const Op& op : small)
        out += "    " + to_string(op) + ",\n";
    return out;
}

} // namespace property
