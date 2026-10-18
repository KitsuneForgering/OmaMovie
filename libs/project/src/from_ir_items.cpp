#include "from_ir_detail.hpp"

#include "oma/timeline/edit.hpp"

#include <format>

namespace oma::project::from_ir_detail {

namespace tl = oma::timeline;

void issue(ir::ImportReport& report, std::string object, std::string reason, std::string impact) {
    report.issues.push_back({.object = std::move(object),
                             .reason = std::move(reason),
                             .level = ir::SupportLevel::Edits,
                             .impact = std::move(impact)});
}

Result<void> place_items(tl::Editor& ed, tl::TrackId track, const ir::Track& source,
                         const MediaIds& media, ir::ImportReport& r) {
    std::int64_t at = 0;
    const ir::Transition* pending = nullptr; // OTIO transitions take no track time
    for (const ir::Item& item : source.items) {
        if (const auto* gap = std::get_if<ir::Gap>(&item))
            at += ticks_of(ed.timeline(), gap->duration, source.name, r);
        if (const auto* t = std::get_if<ir::Transition>(&item))
            pending = t;
        const auto* clip = std::get_if<ir::Clip>(&item);
        if (clip == nullptr)
            continue;
        const std::int64_t length = ticks_of(ed.timeline(), clip->source.duration(), clip->name, r);
        const auto m = media.find(clip->media);
        const tl::ClipId id = ed.new_clip_id();
        const tl::edit::ClipSource placed{.media = m != media.end() ? m->second : tl::MediaId{},
                                          .source_in = clip->source.start(),
                                          .duration = ed.timeline().at(length),
                                          .time_map = {},
                                          .video = {},
                                          .audio = {}};
        if (m == media.end() ||
            !ed.execute(tl::edit::overwrite(track, id, ed.timeline().at(at), placed)))
            issue(r, clip->name, "clip cannot be placed (no media, or outside it)",
                  "left as a gap of its length");
        else if (pending != nullptr)
            transition_into(ed, id, *pending, clip->name, r);
        pending = nullptr;
        at += length;
    }
    return {};
}

} // namespace oma::project::from_ir_detail
