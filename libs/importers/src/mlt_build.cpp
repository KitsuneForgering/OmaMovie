#include "mlt_detail.hpp"

namespace oma::importers::mlt {

namespace {

// Kdenlive wraps each track in a tractor of playlists: those contribute their own tracks.
void track_of(Builder& b, const TrackRef& ref, std::size_t depth) {
    if (const auto p = b.doc.playlists.find(ref.producer); p != b.doc.playlists.end()) {
        const bool audio = ref.audio || p->second.audio;
        ir::Track track{.name = ref.producer,
                        .kind = audio ? ir::TrackKind::Audio : ir::TrackKind::Video,
                        .items = {},
                        .unsupported = {}};
        items_of(b, p->second, track);
        if (!track.items.empty())
            b.out.timeline.tracks.push_back(std::move(track));
        return;
    }
    const auto t = b.doc.tractors.find(ref.producer);
    if (t == b.doc.tractors.end() || depth > 4) {
        issue(b.out.report, ref.producer, "track is not a playlist", "left out");
        return;
    }
    for (const ir::Opaque& kept : t->second.transitions)
        b.out.timeline.unsupported.push_back(kept);
    for (const TrackRef& sub : t->second.tracks)
        track_of(b, {.producer = sub.producer, .audio = ref.audio || sub.audio, .in = 0, .out = -1},
                 depth + 1);
}

} // namespace

Result<void> build(const Document& doc, Imported& out) {
    if (doc.main.empty())
        return {};
    Builder b{.doc = doc, .out = out, .media = {}};
    const Tractor& main = doc.tractors.at(doc.main);
    for (const TrackRef& ref : main.tracks)
        track_of(b, ref, 0);
    for (const ir::Opaque& kept : main.transitions) {
        out.timeline.unsupported.push_back(kept);
        issue(out.report, kept.original_id.empty() ? kept.schema : kept.original_id,
              kept.schema + " between tracks not imported", "kept in the import");
    }
    return {};
}

} // namespace oma::importers::mlt
