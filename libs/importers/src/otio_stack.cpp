#include "otio_detail.hpp"

#include "oma/project_ir/validate.hpp"

#include <format>

namespace oma::importers::otio {

namespace {
Result<ir::Track> track_of(Object t, ir::Timeline& tl, Context& c) {
    ir::Track track;
    track.name = text(t, "name");
    track.kind = text(t, "kind") == "Audio" ? ir::TrackKind::Audio : ir::TrackKind::Video;
    simdjson::dom::array children;
    if (t["children"].get(children) != simdjson::SUCCESS)
        return track;
    for (Element const e : children) {
        Object o;
        if (e.get(o) != simdjson::SUCCESS)
            return make_error(ErrorCode::InvalidData, Category::Importer,
                              "a track child is not an object", track.name);
        auto item = item_of(o, tl, c);
        if (!item)
            return std::unexpected(item.error());
        if (*item)
            track.items.push_back(std::move(**item));
        else
            track.unsupported.push_back(opaque(e, text(o, "name")));
    }
    return track;
}
} // namespace

Result<void> read_stack(Object top, Imported& out, Context& c) {
    Object stack;
    simdjson::dom::array children;
    if (top["tracks"].get(stack) != simdjson::SUCCESS || !schema_of(stack).starts_with("Stack.") ||
        stack["children"].get(children) != simdjson::SUCCESS)
        return make_error(ErrorCode::InvalidData, Category::Importer, "timeline has no track stack",
                          out.timeline.name);
    for (Element const e : children) {
        Object o;
        if (e.get(o) == simdjson::SUCCESS && schema_of(o).starts_with("Track.")) {
            auto track = track_of(o, out.timeline, c);
            if (!track)
                return std::unexpected(track.error());
            out.timeline.tracks.push_back(std::move(*track));
            continue;
        }
        const ir::Opaque& kept = out.timeline.unsupported.emplace_back(opaque(e, {}));
        c.issue(kept.original_id, std::format("{} in the top stack is not imported", kept.schema),
                "kept in the import; nested stacks are not flattened");
    }
    if (auto valid = ir::validate(out.timeline); !valid)
        return std::unexpected(valid.error());
    out.report.reached = ir::SupportLevel::Edits;
    return {};
}

} // namespace oma::importers::otio
