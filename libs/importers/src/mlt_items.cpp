#include "mlt_detail.hpp"

namespace oma::importers::mlt {

namespace {

// melt's mix: a tractor of the outgoing and incoming producers' overlapping frames. The previous
// clip takes its own frames on; a dissolve of the overlap's length ends at the next entry.
bool mix(Builder& b, const Tractor& t, ir::Track& track) {
    auto* before = track.items.empty() ? nullptr : std::get_if<ir::Clip>(&track.items.back());
    if (before == nullptr || t.tracks.size() != 2 || t.length <= 0)
        return false;
    before->source =
        *TimeRange::make(before->source.start(), *before->source.duration().plus(at(b, t.length)));
    track.items.emplace_back(ir::Transition{.kind = "SMPTE_Dissolve",
                                            .in_offset = at(b, t.length),
                                            .out_offset = at(b, 0),
                                            .unsupported = t.transitions});
    return true;
}

} // namespace

void items_of(Builder& b, const Playlist& playlist, ir::Track& track) {
    for (const Entry& e : playlist.entries) {
        const std::int64_t length = e.out - e.in + 1;
        if (length <= 0)
            continue;
        const auto p = b.doc.producers.find(e.producer);
        const auto t = b.doc.tractors.find(e.producer);
        std::optional<ir::Clip> clip;
        if (p != b.doc.producers.end())
            clip = clip_of(b, p->second, e.in, e.out);
        if (clip)
            track.items.emplace_back(std::move(*clip));
        else if (t == b.doc.tractors.end() || !mix(b, t->second, track)) {
            if (!e.producer.empty())
                issue(b.out.report, e.producer, why_not(b, e.producer), "left as a gap");
            track.items.emplace_back(ir::Gap{.duration = at(b, length)});
        }
    }
}

} // namespace oma::importers::mlt
