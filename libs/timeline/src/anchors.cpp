#include "anchors.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace oma::timeline::detail {

namespace {

bool hosts(const Timeline& tl, const Clip& p, const RationalTime& source) {
    return p.time_map.is_constant() && !p.anchor && holds(p, source, tl.timebase());
}

// The clip that now shows the anchor's source: the primary itself, or a clip the edit created
// from it (the tail of a split: new ID, same track and media).
const Clip* host_of(const Timeline& tl, const Anchor& a, const Places& before) {
    if (const Clip* p = tl.find_clip(a.primary); p != nullptr && hosts(tl, *p, a.source)) {
        return p;
    }
    const auto was = before.find(a.primary);
    if (was == before.end()) {
        return nullptr;
    }
    const Track* track = tl.find_track(was->second.track);
    if (track == nullptr) {
        return nullptr;
    }
    for (const Clip& c : track->clips) {
        if (!before.contains(c.id) && c.media == was->second.media && hosts(tl, c, a.source)) {
            return &c;
        }
    }
    return nullptr;
}

bool moved(const Clip& c, const Track& t, const ClipPlace& p) {
    return t.id != p.track || c.start_ticks() != p.start || c.duration.value() != p.duration ||
           c.source_in != p.source_in;
}

} // namespace

bool has_anchors(const Timeline& tl) {
    return std::ranges::any_of(tl.tracks(), [](const Track& t) {
        return std::ranges::any_of(t.clips, [](const Clip& c) { return c.anchor.has_value(); });
    });
}

Places places(const Timeline& tl) {
    Places out;
    for (const Track& t : tl.tracks()) {
        for (const Clip& c : t.clips) {
            out.emplace(c.id, ClipPlace{.track = t.id,
                                        .media = c.media,
                                        .start = c.start_ticks(),
                                        .duration = c.duration.value(),
                                        .source_in = c.source_in});
        }
    }
    return out;
}

Result<Steps> reattach(const Timeline& tl, const Places& before) {
    // Dependents that move are erased first and inserted again at the end, so moving several
    // on one track never overlaps their own old places in between.
    Steps erase;
    Steps insert;
    Steps update;
    for (const Track& t : tl.tracks()) {
        for (const Clip& d : t.clips) {
            if (!d.anchor) {
                continue;
            }
            Clip next = d;
            const auto was = before.find(d.id);
            const Clip* primary = tl.find_clip(d.anchor->primary);
            if (was == before.end() || moved(d, t, was->second)) {
                // The edit moved or made the dependent itself: connect it where it now starts,
                // or let it go if it no longer starts on the primary.
                next.anchor.reset();
                if (primary != nullptr && tl.track_of(primary->id)->id != t.id &&
                    primary->time_map.is_constant() && !primary->anchor &&
                    d.start_ticks() >= primary->start_ticks() &&
                    d.start_ticks() < primary->end_ticks()) {
                    auto source = source_at(*primary, d.start_ticks(), tl.timebase());
                    if (!source) {
                        return std::unexpected(source.error());
                    }
                    next.anchor = Anchor{.primary = primary->id, .source = *source};
                }
                if (next.anchor != d.anchor) {
                    update.push_back(replace_clip(std::move(next)));
                }
                continue;
            }
            if (const Clip* host = host_of(tl, *d.anchor, before); host != nullptr) {
                auto at = attach_tick(*host, d.anchor->source, tl.timebase());
                if (!at) {
                    return std::unexpected(at.error());
                }
                next.anchor->primary = host->id;
                if (*at != d.start_ticks()) {
                    next.start = tl.at(*at);
                    erase.push_back(erase_clip(d.id));
                    insert.push_back(insert_clip(t.id, std::move(next)));
                } else if (next.anchor != d.anchor) {
                    update.push_back(replace_clip(std::move(next)));
                }
            } else if (primary == nullptr) {
                erase.push_back(erase_clip(d.id)); // the primary was removed: so is its dependent
            } else {
                next.anchor.reset(); // its content was trimmed away or is no longer constant speed
                update.push_back(replace_clip(std::move(next)));
            }
        }
    }
    Steps steps;
    for (auto* group : {&update, &erase, &insert}) {
        std::ranges::move(*group, std::back_inserter(steps));
    }
    return steps;
}

} // namespace oma::timeline::detail
