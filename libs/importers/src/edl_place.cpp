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
    const std::string target = !e.source_file.empty() ? e.source_file
                               : !e.clip_name.empty() ? e.clip_name
                                                      : e.reel;
    auto [m, fresh] = b.media.try_emplace(target, std::format("m{}", b.media.size() + 1));
    if (fresh)
        b.out.timeline.media.push_back(
            {.id = m->second, .target = target, .available = {}, .unsupported = {}});
    ir::Clip clip{.name = e.clip_name.empty() ? e.reel : e.clip_name,
                  .media = m->second,
                  .source = *TimeRange::make(at(e.src_in), at(e.src_out - e.src_in)),
                  .unsupported = {}};
    for (const std::string& note : e.notes) {
        clip.unsupported.push_back(note_of(e, note));
        issue(b.out.report, e.number,
              note.starts_with("M2") ? "speed change (M2) not imported" : "comment kept",
              note.starts_with("M2") ? "plays at normal speed; length kept" : "kept in the import");
    }
    track.items.emplace_back(std::move(clip));
    cursor = e.rec_out;
}

} // namespace oma::importers::edl
