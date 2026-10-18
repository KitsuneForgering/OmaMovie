// FCPXML import of basic.fcpxml (hand-written 1.10): cuts, a dissolve, a gap, lane 1, a title.
#include "oma/importers/fcpxml.hpp"
#include "otio_test.hpp"

#include <algorithm>
#include <variant>

#include "oma_test.hpp"

namespace {

std::int64_t frames(const oma::RationalTime& t) { // at 29.97 fps
    return t.rescaled(*oma::Rational::make(1001, 30000), oma::Rounding::Nearest)->value();
}

bool reads_the_spine() {
    const auto r = oma::importers::import_fcpxml(read_fixture("basic.fcpxml"));
    if (!r || r->report.format != "FCPXML" || r->report.version != "1.10" ||
        r->report.reached != oma::ir::SupportLevel::Edits || r->timeline.name != "Cuts" ||
        r->timeline.tracks.size() != 2 || r->timeline.media.size() != 1)
        return false;
    const auto& items = r->timeline.tracks[0].items;
    const auto* a = items.size() == 4 ? std::get_if<oma::ir::Clip>(&items[0]) : nullptr;
    const auto* mix = a ? std::get_if<oma::ir::Transition>(&items[1]) : nullptr;
    const auto* b = a ? std::get_if<oma::ir::Clip>(&items[2]) : nullptr;
    const auto* gap = a ? std::get_if<oma::ir::Gap>(&items[3]) : nullptr;
    const auto& media = r->timeline.media[0];
    return a && mix && b && gap && frames(a->source.start()) == 30 &&
           frames(a->source.duration()) == 60 && a->unsupported.size() == 1 &&
           mix->kind == "SMPTE_Dissolve" && frames(mix->in_offset) == 6 &&
           frames(mix->out_offset) == 6 && frames(b->source.start()) == 150 &&
           frames(gap->duration) == 30 && media.target == "file:///media/clip.mp4" &&
           media.available && frames(media.available->duration()) == 300;
}

// Clip C is at B's own time 180180/30000 s, 30 frames into B, so at frame 90 of the timeline.
bool connects_lane_one() {
    const auto r = oma::importers::import_fcpxml(read_fixture("basic.fcpxml"));
    if (!r || r->timeline.tracks.size() != 2)
        return false;
    const auto& items = r->timeline.tracks[1].items;
    const auto* gap = items.size() == 2 ? std::get_if<oma::ir::Gap>(&items[0]) : nullptr;
    const auto* c = gap ? std::get_if<oma::ir::Clip>(&items[1]) : nullptr;
    const bool title_reported = std::ranges::any_of(
        r->report.issues, [](const oma::ir::ImportIssue& i) { return i.object == "Hello"; });
    return c && frames(gap->duration) == 90 && c->name == "C" &&
           frames(c->source.duration()) == 15 && title_reported;
}

} // namespace

void run_fcpxml_tests() {
    describe("FCPXML import", {
        it("reads the spine: cuts, a centred dissolve, a gap, shared media",
           { expect(reads_the_spine()).toBeTruthy(); });
        it("lays connected clips on their lane's track",
           { expect(connects_lane_one()).toBeTruthy(); });
    });
}
