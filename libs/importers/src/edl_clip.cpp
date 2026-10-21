#include "edl_tracks.hpp"

#include <format>

namespace oma::importers::edl {

ir::Clip clip_of(Build& b, const Event& e) {
    const auto at = [&](std::int64_t frames) {
        return *RationalTime::make(frames, b.timebase);
    };
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
    return clip;
}

} // namespace oma::importers::edl
