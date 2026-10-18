#include "property_gen.hpp"

namespace property {

using namespace oma::timeline;

namespace {

// The index of the first clip on `track` covering `ticks`, or -1.
int covering(const Track& track, std::int64_t ticks) {
    for (std::size_t i = 0; i < track.clips.size(); ++i)
        if (track.clips[i].start_ticks() <= ticks && ticks < track.clips[i].end_ticks())
            return static_cast<int>(i);
    return -1;
}

} // namespace

void aim(Rng& rng, const Timeline& tl, Op& op) {
    const auto tracks = tl.tracks();
    if (op.track >= static_cast<int>(tracks.size()) || rng.between(0, 4) == 0)
        return; // keep some unaimed steps: their rejections are part of the property
    const Track& track = tracks[static_cast<std::size_t>(op.track)];
    if (op.kind == Kind::Roll) {
        // A clip whose end meets the next clip's start, if the track has one.
        for (std::size_t i = 0; i + 1 < track.clips.size(); ++i)
            if (track.clips[i].end_ticks() == track.clips[i + 1].start_ticks() &&
                rng.between(0, 1) == 0) {
                op.clip = static_cast<int>(i);
                break;
            }
    } else if (op.kind == Kind::Connect && op.clip < static_cast<int>(track.clips.size())) {
        // A primary on another track under the dependent's start.
        const std::int64_t start = track.clips[static_cast<std::size_t>(op.clip)].start_ticks();
        for (std::size_t t = 0; t < tracks.size(); ++t)
            if (static_cast<int>(t) != op.track) {
                if (const int i = covering(tracks[t], start); i >= 0) {
                    op.b = static_cast<std::int64_t>(t);
                    op.c = i;
                    break;
                }
            }
    }
}

} // namespace property
