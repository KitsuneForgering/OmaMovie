// MLT XML import against a file melt 7.40 wrote itself (melt_mix.mlt: frames 5-19 and 0-9 of one
// clip with a five-frame luma mix, `melt A in=5 out=19 A in=0 out=9 -mix 5 -mixer luma`).
#include "otio_test.hpp"

#include "oma/importers/mlt.hpp"

#include <variant>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

std::int64_t frames(const oma::RationalTime& t) {
    const auto r = t.rescaled(oma::Rational::make(1, 30).value(), oma::Rounding::Nearest);
    return r ? r->value() : -1;
}

// melt rendered the same document with the outgoing clip running on under the mix: 15 frames
// from source frame 5, a 5-frame dissolve, then the incoming clip's last 5 frames (5-9).
bool reads_melts_own_mix() {
    const auto r = oma::importers::import_mlt(read_fixture("melt_mix.mlt"));
    if (!r || r->report.format != "MLT XML" || r->report.version != "7.40.0" ||
        r->report.reached != ir::SupportLevel::Edits || r->timeline.tracks.size() != 1 ||
        r->timeline.media.size() != 1)
        return false;
    const auto& items = r->timeline.tracks[0].items;
    const auto* a = items.size() == 3 ? std::get_if<ir::Clip>(&items[0]) : nullptr;
    const auto* mix = a ? std::get_if<ir::Transition>(&items[1]) : nullptr;
    const auto* b = a ? std::get_if<ir::Clip>(&items[2]) : nullptr;
    const auto& media = r->timeline.media[0];
    return a && mix && b && frames(a->source.start()) == 5 && frames(a->source.duration()) == 15 &&
           frames(mix->in_offset) == 5 && frames(b->source.start()) == 5 &&
           frames(b->source.duration()) == 5 && media.available &&
           frames(media.available->duration()) == 30 &&
           media.target == "/media/project/tests/fixtures/generated/h264_30fps_aac.mp4";
}

bool refused(const char* text) {
    return !oma::importers::import_mlt(text);
}

} // namespace

void run_mlt_tests() {
    describe("MLT import", {
        it("reads melt's own document: cuts, a mix as a dissolve, shared media",
           { expect(reads_melts_own_mix()).toBeTruthy(); });
        it("refuses what is not MLT", {
            expect(refused("<kdenlive/>")).toBeTruthy();
            expect(refused("<mlt><profile frame_rate_num=\"0\" frame_rate_den=\"1\"/></mlt>"))
                .toBeTruthy();
            expect(refused("not xml")).toBeTruthy();
        });
    });
}
