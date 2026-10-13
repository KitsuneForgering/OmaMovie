#include "otio_detail.hpp"

#include <format>

namespace oma::importers::otio {

Result<ir::Item> clip_of(Object o, const std::string& name, ir::Timeline& tl, Context& c) {
    auto media = media_of(o, tl, c);
    if (!media)
        return std::unexpected(media.error());
    Element range;
    if (o["source_range"].get(range) != simdjson::SUCCESS || range.is_null())
        return make_error(ErrorCode::Unsupported, Category::Importer, "clip without a source range",
                          name);
    auto source = range_of(range, c, name);
    if (!source)
        return std::unexpected(source.error());
    ir::Clip clip{.name = name, .media = *media, .source = *source, .unsupported = {}};
    // Effects and markers are kept for a later level (CLAUDE.md §16.3), not applied.
    for (const char* key : {"effects", "markers"}) {
        simdjson::dom::array list;
        if (o[key].get(list) != simdjson::SUCCESS)
            continue;
        for (Element e : list) {
            clip.unsupported.push_back(opaque(e, name));
            c.issue(name, std::format("{} {} not imported", key, clip.unsupported.back().schema),
                    "kept in the import, not applied", ir::SupportLevel::Edits);
        }
    }
    return clip;
}

} // namespace oma::importers::otio
