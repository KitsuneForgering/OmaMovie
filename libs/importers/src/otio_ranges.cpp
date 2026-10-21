#include "otio_detail.hpp"

#include <optional>
#include <unordered_map>

namespace oma::importers::otio {

void trust_available(ir::Timeline& tl, Context& c) {
    std::unordered_map<std::string, ir::MediaRef*> by_id;
    for (ir::MediaRef& m : tl.media)
        by_id.emplace(m.id, &m);
    for (const ir::Track& track : tl.tracks)
        for (const ir::Item& item : track.items) {
            const auto* clip = std::get_if<ir::Clip>(&item);
            const auto m = clip ? by_id.find(clip->media) : by_id.end();
            if (m == by_id.end())
                continue;
            std::optional<TimeRange>& available = m->second->available;
            if (available && (clip->source.start() < available->start() ||
                              available->end() < clip->source.end())) {
                c.issue(clip->name, "clip runs outside its media's available_range",
                        "range ignored; the file's length is used");
                available.reset();
            }
        }
}

} // namespace oma::importers::otio
