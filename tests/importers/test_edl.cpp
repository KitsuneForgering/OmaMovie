#include "otio_test.hpp"

#include "oma/importers/edl.hpp"

#include <algorithm>
#include <variant>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

std::int64_t frames(const oma::RationalTime& t) {
    const auto r = t.rescaled(oma::Rational::make(1, 30).value(), oma::Rounding::Nearest);
    return r ? r->value() : -1;
}

bool reported(const ir::ImportReport& r, std::string_view text) {
    return std::ranges::any_of(r.issues, [&](const ir::ImportIssue& i) {
        return i.reason.find(text) != std::string::npos;
    });
}

// basic.edl: a cut, a 12-frame dissolve, AA sound with a speed line, then a wipe after a gap.
bool reads_the_basic_list() {
    const auto r =
        oma::importers::import_edl(read_fixture("basic.edl"), oma::Rational::make(30, 1).value());
    if (!r || r->report.reached != ir::SupportLevel::Edits || r->timeline.name != "OMA EDL TEST" ||
        frames(*r->timeline.global_start) != 108000 || r->timeline.tracks.size() != 3 ||
        r->timeline.media.size() != 3)
        return false;
    const auto& v = r->timeline.tracks[0];
    const auto& a1 = r->timeline.tracks[1];
    const auto* first = v.items.size() == 5 ? std::get_if<ir::Clip>(&v.items[0]) : nullptr;
    const auto* dissolve = first ? std::get_if<ir::Transition>(&v.items[1]) : nullptr;
    const auto* gap = first ? std::get_if<ir::Gap>(&v.items[3]) : nullptr;
    const auto* sound = a1.items.size() == 2 ? std::get_if<ir::Clip>(&a1.items[1]) : nullptr;
    return v.name == "V" && a1.kind == ir::TrackKind::Audio && first && dissolve && gap && sound &&
           r->timeline.media[0].target == "/media/camera.mov" && sound->media == first->media &&
           frames(first->source.start()) == 30 && frames(first->source.duration()) == 60 &&
           dissolve->kind == "SMPTE_Dissolve" && frames(dissolve->out_offset) == 12 &&
           frames(gap->duration) == 60 && reported(r->report, "M2") &&
           reported(r->report, "W001") && !v.unsupported.empty();
}

// 00:01:00;02 is the first frame number of minute one in drop-frame: frame 1800.
bool reads_drop_frame() {
    const char* text = "FCM: DROP FRAME\n"
                       "001 AX V C 00:01:00;02 00:01:01;00 00:00:00;00 00:00:00;28\n";
    const auto r = oma::importers::import_edl(text, oma::Rational::make(30000, 1001).value());
    const auto* clip = r && !r->timeline.tracks.empty()
                           ? std::get_if<ir::Clip>(&r->timeline.tracks[0].items[0])
                           : nullptr;
    const auto tb = oma::Rational::make(1001, 30000).value();
    const auto start = clip ? clip->source.start().rescaled(tb, oma::Rounding::Nearest)
                            : oma::Result<oma::RationalTime>(std::unexpected(oma::Error(
                                  oma::ErrorCode::Internal, oma::Category::Importer, "none")));
    return start && start->value() == 1800;
}

} // namespace

void run_edl_tests() {
    describe("EDL import", {
        it("reads cuts, a dissolve, AA sound, gaps and reports what it cannot map",
           { expect(reads_the_basic_list()).toBeTruthy(); });
        it("reads drop-frame timecode exactly", { expect(reads_drop_frame()).toBeTruthy(); });
    });
}
