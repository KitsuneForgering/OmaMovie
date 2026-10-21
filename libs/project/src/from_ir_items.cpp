#include "from_ir_dissolve.hpp"

#include "oma/timeline/edit.hpp"

#include <format>
#include <utility>
#include <vector>

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

Result<void> place_items(tl::Editor& ed, tl::TrackId track, const ir::Track& source,
                         const MediaIds& media, ir::ImportReport& r) {
    std::int64_t at = 0;
    const ir::Transition* pending = nullptr; // OTIO transitions take no track time
    std::vector<Edit> clips;
    std::vector<std::pair<const ir::Clip*, tl::ClipId>> placed; // with the transition into each
    std::vector<const ir::Transition*> into;
    for (const ir::Item& item : source.items) {
        if (const auto* gap = std::get_if<ir::Gap>(&item))
            at += ticks_of(ed.timeline(), gap->duration, source.name, r);
        if (const auto* t = std::get_if<ir::Transition>(&item))
            pending = t;
        const auto* clip = std::get_if<ir::Clip>(&item);
        if (clip == nullptr)
            continue;
        const std::int64_t length = ticks_of(ed.timeline(), played(*clip), clip->name, r);
        const auto m = media.find(clip->media);
        const auto map = tl::TimeMap::constant(clip->speed);
        if (m == media.end() || !map) {
            issue(r, clip->name, "clip cannot be placed (no media, or outside it)",
                  "left as a gap of its length");
        } else {
            const tl::ClipId id = ed.new_clip_id();
            const tl::edit::ClipSource from{.media = m->second,
                                            .source_in = clip->source.start(),
                                            .duration = ed.timeline().at(length),
                                            .time_map = *map,
                                            .video = {},
                                            .audio = {}};
            clips.emplace_back([track, id, start = ed.timeline().at(at), from] {
                return tl::edit::overwrite(track, id, start, from);
            });
            placed.emplace_back(clip, id);
            into.push_back(pending);
        }
        pending = nullptr;
        at += length;
    }
    std::vector<std::uint8_t> landed(clips.size());
    execute_each(ed, clips, landed);
    place_dissolves(ed, placed, landed, into, r);
    return {};
}

} // namespace oma::project::from_ir_detail
