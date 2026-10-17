#include "property_invariants.hpp"

#include <format>
#include <set>

namespace property {

using namespace oma::timeline;

namespace {

bool on_grid(const Timeline& tl, const oma::RationalTime& t) {
    return tl.to_ticks(t).has_value();
}

std::string broken_clip(const Timeline& tl, const Track& track, const Clip& c) {
    if (!on_grid(tl, c.start) || !on_grid(tl, c.duration) || c.start_ticks() < 0 ||
        c.duration.value() <= 0)
        return std::format("clip {}: off the grid, negative or empty", c.id.value());
    const MediaInfo* m = tl.find_media(c.media);
    const bool plays = track.kind == TrackKind::Audio ? m && m->has_audio
                                                      : (m && m->has_video) || c.title.has_value();
    if (!plays)
        return std::format("clip {}: media the track cannot play", c.id.value());
    if (c.anchor) {
        const Clip* primary = tl.find_clip(c.anchor->primary);
        if (!primary || tl.track_of(primary->id) == &track || primary->anchor)
            return std::format("clip {}: invalid anchor", c.id.value());
    }
    return broken_extent(tl, c);
}

} // namespace

std::string broken_structure(const Timeline& tl) {
    std::set<std::uint64_t> tracks;
    std::set<std::uint64_t> clips;
    for (const Track& track : tl.tracks()) {
        if (!tracks.insert(track.id.value()).second)
            return std::format("track {}: duplicate ID", track.id.value());
        std::int64_t end = 0;
        for (const Clip& c : track.clips) {
            if (!clips.insert(c.id.value()).second)
                return std::format("clip {}: duplicate ID", c.id.value());
            if (c.start_ticks() < end)
                return std::format("clip {}: overlaps or precedes the clip before", c.id.value());
            end = c.end_ticks();
            if (auto broken = broken_clip(tl, track, c); !broken.empty())
                return broken;
        }
    }
    for (const Marker& m : tl.markers())
        if (!on_grid(tl, m.time))
            return std::format("marker {}: off the grid", m.id.value());
    return {};
}

} // namespace property
