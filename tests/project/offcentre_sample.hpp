#pragma once

// One cut with a 12-frame dissolve between clips of m1, imported, read back in frames.

#include "from_ir_sample.hpp"

#include <algorithm>
#include <string>

namespace from_ir_sample {

struct Cut {
    std::int64_t a_frames = -1, b_start = -1, b_in = -1, b_frames = -1, dissolve = -1; // frames
    std::string impact; // the report's word on the dissolve
};

// a from source 0, a 12-frame dissolve `before`/`after` its cut, then b from `b_in`.
inline Cut imported(std::int64_t a_frames, std::int64_t before, std::int64_t after,
                    std::int64_t b_in, std::int64_t b_frames) {
    ir::Timeline t;
    t.media = {{.id = "m1", .target = "a.mov", .available = std::nullopt, .unsupported = {}}};
    ir::Track v1{.name = "V1", .kind = ir::TrackKind::Video, .items = {}, .unsupported = {}};
    v1.items = {clip("a", "m1", 0, a_frames),
                ir::Transition{.kind = "SMPTE_Dissolve",
                               .in_offset = f24(before),
                               .out_offset = f24(after),
                               .unsupported = {}},
                clip("b", "m1", b_in, b_frames)};
    t.tracks.push_back(std::move(v1));
    ir::ImportReport report;
    const auto doc = oma::project::from_ir(t, resolve, report);
    const oma::timeline::Track* track =
        doc && doc->timeline ? doc->timeline->find_track(doc->storyline) : nullptr;
    if (track == nullptr || track->clips.size() != 2 || !track->clips[1].transition_in)
        return {};
    const auto frames = [](const oma::RationalTime& x) {
        const auto f = x.rescaled(oma::Rational::make(1, 24).value(), oma::Rounding::Nearest);
        return f ? f->value() : -1;
    };
    const oma::timeline::Clip& b = track->clips[1];
    const auto said = std::ranges::find_if(report.issues, [](const ir::ImportIssue& i) {
        return i.reason == "dissolve is not centred on the cut";
    });
    return {frames(track->clips[0].duration),
            frames(b.start),
            frames(b.source_in),
            frames(b.duration),
            frames(b.transition_in->duration),
            said == report.issues.end() ? "" : said->impact};
}

} // namespace from_ir_sample
