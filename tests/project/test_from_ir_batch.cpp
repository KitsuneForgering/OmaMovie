// A long imported track: clips are placed in groups (one validation per group), and a clip that
// cannot be placed still fails on its own, leaving every other clip and dissolve in place.
#include "from_ir_sample.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace ir = oma::ir;
namespace tl = oma::timeline;
using namespace from_ir_sample;

namespace {

// 300 cuts of 12 frames from m1 (240 frames). Clip 137 starts past the media's end, so the group
// holding it fails and is halved down to it; clips 100 and 200 have dissolves into them.
bool failures_stay_local() {
    ir::Timeline t;
    t.media = {{.id = "m1", .target = "a.mov", .available = std::nullopt, .unsupported = {}}};
    ir::Track v1{.name = "V1", .kind = ir::TrackKind::Video, .items = {}, .unsupported = {}};
    for (int i = 0; i < 300; ++i) {
        if (i == 100 || i == 200)
            v1.items.emplace_back(ir::Transition{.kind = "SMPTE_Dissolve",
                                                 .in_offset = f24(12),
                                                 .out_offset = f24(12),
                                                 .unsupported = {}});
        v1.items.emplace_back(
            clip(i == 137 ? "past" : "c", "m1", i == 137 ? 250 : 12 + (i % 200), 12));
    }
    t.tracks.push_back(std::move(v1));
    ir::ImportReport report;
    const auto doc = oma::project::from_ir(t, resolve, report);
    const tl::Track* track =
        doc && doc->timeline ? doc->timeline->find_track(doc->storyline) : nullptr;
    if (track == nullptr || track->clips.size() != 299)
        return false; // 24 fps on 1/48000 below: 2000 ticks a frame
    bool in_place = true;
    for (std::size_t k = 0; k < track->clips.size(); ++k) {
        const std::size_t i = k < 137 ? k : k + 1; // the source index, past the missing clip
        in_place = in_place && track->clips[k].start ==
                                   doc->timeline->at(static_cast<std::int64_t>(i) * 12 * 2000);
    }
    const auto said = [&](const char* who, const char* reason) {
        return std::ranges::any_of(report.issues, [&](const ir::ImportIssue& i) {
            return i.object == who && i.reason == reason;
        });
    };
    return in_place && report.issues.size() == 1 &&
           said("past", "clip cannot be placed (no media, or outside it)") &&
           track->clips[100].transition_in.has_value() && track->clips[199].transition_in &&
           !track->clips[198].transition_in;
}

} // namespace

void run_from_ir_batch_tests() {
    describe("project::from_ir on a long track", {
        it("keeps a failed clip or dissolve local to itself",
           { expect(failures_stay_local()).toBeTruthy(); });
    });
}
