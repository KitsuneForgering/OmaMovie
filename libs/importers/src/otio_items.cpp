#include "otio_detail.hpp"

#include <format>

namespace oma::importers::otio {

Result<std::optional<ir::Item>> item_of(Object o, ir::Timeline& tl, Context& c) {
    if (++c.items > kMaxItems)
        return make_error(ErrorCode::OutOfRange, Category::Importer, "too many timeline items", "");
    const std::string schema = schema_of(o);
    const std::string name = text(o, "name");
    if (schema.starts_with("Clip.1") || schema.starts_with("Clip.2")) {
        auto clip = clip_of(o, name, tl, c);
        if (!clip || !disabled(o))
            return clip;
        c.issue(name, "clip disabled in the source", "left as a gap of its length");
        return ir::Item{ir::Gap{.duration = std::get<ir::Clip>(*clip).source.duration()}};
    }
    Element range;
    if (schema.starts_with("Gap.") && o["source_range"].get(range) == simdjson::SUCCESS) {
        auto r = range_of(range, c, name);
        if (!r)
            return std::unexpected(r.error());
        return ir::Item{ir::Gap{.duration = r->duration()}};
    }
    Element in;
    Element out;
    if (schema.starts_with("Transition.") && o["in_offset"].get(in) == simdjson::SUCCESS &&
        o["out_offset"].get(out) == simdjson::SUCCESS) {
        auto a = time_of(in, c, name);
        auto b = time_of(out, c, name);
        if (!a || !b)
            return std::unexpected(!a ? a.error() : b.error());
        return ir::Item{ir::Transition{.kind = text(o, "transition_type"),
                                       .in_offset = *a,
                                       .out_offset = *b,
                                       .unsupported = {}}};
    }
    c.issue(name.empty() ? schema : name, std::format("{} is not imported", schema),
            "kept in the import; the track may be shorter");
    return std::optional<ir::Item>{};
}

} // namespace oma::importers::otio
