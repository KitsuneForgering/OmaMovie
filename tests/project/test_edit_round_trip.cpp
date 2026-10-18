// Every state the edit properties generate (tests/timeline/property_*.cpp, M10) saves and
// reloads unchanged: the project format is checked against the test-only canonical state, not
// used as its own oracle (docs/implementation-plan-qa-timeline.md).
#include "oma/project/document.hpp"
#include "property_checker.hpp"
#include "property_gen.hpp"

#include <format>
#include <string>

#include "oma_test.hpp"

namespace {

namespace pj = oma::project;

// The state after a save and a load, or why it could not round trip.
std::string reloaded(const oma::timeline::Timeline& tl) {
    pj::Document doc;
    for (const auto& m : tl.media())
        doc.media.push_back(pj::MediaRef{.info = m,
                                         .path = std::format("/property/{}.mp4", m.id.value()),
                                         .name = "m",
                                         .audio_only = !m.has_video,
                                         .fingerprint = {}});
    doc.timeline = tl;
    doc.storyline = tl.tracks().front().id;
    auto json = pj::to_json(doc, "/property");
    auto back = json ? pj::from_json(*json, "/property") : std::unexpected(json.error());
    if (!back || !back->timeline)
        return "round trip failed: " + (back ? std::string("no timeline") : back.error().message());
    return property::difference(property::canonical(tl), property::canonical(*back->timeline));
}

// Seeds 1…16 of 60 steps; the first state that changes on the way, with its seed.
std::string seeds_round_trip() {
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        property::Checker checker;
        property::Rng rng(seed);
        for (int step = 0; step < 60; ++step) {
            const int before = checker.transitions();
            if (checker.step(property::generate(rng, checker.timeline())))
                return std::format("seed {}: a property failed", seed);
            if (checker.transitions() == before)
                continue;
            if (auto diff = reloaded(checker.timeline()); !diff.empty())
                return std::format("seed {} step {}: {}", seed, step, diff);
        }
    }
    return {};
}

} // namespace

void run_edit_round_trip_tests() {
    describe("Edited states", {
        it("save and reload unchanged", { expect(seeds_round_trip()).toBe(std::string()); });
    });
}
