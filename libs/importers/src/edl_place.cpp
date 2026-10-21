#include "edl_tracks.hpp"

#include <format>

namespace oma::importers::edl {

void place(Build& b, const Event& e, const std::string& name) {
    const auto at = [&](std::int64_t frames) {
        return *RationalTime::make(frames, b.timebase);
    };
    auto [it, made] = b.tracks.try_emplace(name);
    ir::Track& track = it->second;
    if (made) {
        track.name = name;
        track.kind = name == "V" ? ir::TrackKind::Video : ir::TrackKind::Audio;
    }
    std::int64_t& cursor = b.cursor.try_emplace(name, b.start).first->second;
    if (e.kind == "C" && e.rec_out == e.rec_in)
        return; // the outgoing side of the dissolve that follows: nothing on the record
    if (e.rec_in < cursor) {
        issue(b.out.report, e.number, "event overlaps the one before it on " + name, "left out");
        return;
    }
    if (e.rec_in > cursor)
        track.items.emplace_back(ir::Gap{.duration = at(e.rec_in - cursor)});
    if ((e.reel == "BL" || e.reel == "BLK") && e.source_file.empty()) { // CMX 3600 black
        if (e.kind != "C")
            issue(b.out.report, e.number,
                  std::format("transition {} to black not imported", e.kind),
                  "a hard cut to black; the event's length is kept");
        track.items.emplace_back(ir::Gap{.duration = at(e.rec_out - e.rec_in)});
        cursor = e.rec_out;
        return;
    }
    if (e.kind == "D") {
        track.items.emplace_back(ir::Transition{.kind = "SMPTE_Dissolve",
                                                .in_offset = at(0),
                                                .out_offset = at(e.transition),
                                                .unsupported = {}});
    } else if (e.kind != "C") {
        track.unsupported.push_back(event_of(e));
        issue(b.out.report, e.number, std::format("transition {} not imported", e.kind),
              "a hard cut; the event's length is kept");
    }
    track.items.emplace_back(clip_of(b, e));
    cursor = e.rec_out;
}

} // namespace oma::importers::edl
