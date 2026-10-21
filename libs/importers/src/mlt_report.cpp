#include "mlt_detail.hpp"

#include <algorithm>

namespace oma::importers::mlt {

bool is_a(const ir::Opaque& o, std::string_view element) {
    return std::ranges::any_of(
        o.fields, [&](const auto& f) { return f.first == "element" && f.second == element; });
}

bool inert(const ir::Opaque& o) {
    return o.schema == "audiolevel" || std::ranges::any_of(o.fields, [](const auto& f) {
               return (f.first == "disable" && f.second == "1") ||
                      (f.first == "internal_added" && f.second == "237");
           });
}

void report_transitions(Builder& b, const Tractor& t) {
    // Kdenlive's per-track tractor holds only playlists (A/B): a transition there is a same-track
    // mix. A sequence tractor (Kdenlive 23.04+ nests one in the main tractor) holds tracks.
    const bool pair = std::ranges::all_of(
        t.tracks, [&](const TrackRef& sub) { return b.doc.playlists.contains(sub.producer); });
    for (const ir::Opaque& kept : t.transitions) {
        b.out.timeline.unsupported.push_back(kept);
        if (inert(kept))
            continue;
        const bool mix = is_a(kept, "transition");
        issue(b.out.report, kept.original_id,
              kept.schema + (!mix   ? " track filter not imported"
                             : pair ? " within a track (a same-track mix) not imported"
                                    : " between tracks not imported"),
              mix && pair ? "kept in the import; the clips meet at a cut" : "kept in the import");
    }
}

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact) {
    r.issues.push_back({.object = std::move(object),
                        .reason = std::move(reason),
                        .level = ir::SupportLevel::Edits,
                        .impact = std::move(impact)});
}

} // namespace oma::importers::mlt
