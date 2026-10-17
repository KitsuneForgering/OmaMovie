#include "otio_detail.hpp"

namespace oma::importers::otio {

void Context::issue(std::string object, std::string reason, std::string impact,
                    ir::SupportLevel level) {
    report.issues.push_back({.object = std::move(object),
                             .reason = std::move(reason),
                             .level = level,
                             .impact = std::move(impact)});
}

std::string schema_of(Object o) {
    return text(o, "OTIO_SCHEMA");
}

std::string text(Object o, std::string_view key) {
    std::string_view value;
    return o[key].get(value) == simdjson::SUCCESS ? std::string(value) : std::string();
}

ir::Opaque opaque(Element e, std::string_view id) {
    ir::Opaque out{.format = "otio",
                   .original_id = std::string(id),
                   .schema = {},
                   .raw = simdjson::minify(e),
                   .fields = {}};
    Object o;
    if (e.get(o) == simdjson::SUCCESS) {
        out.schema = schema_of(o);
        if (out.original_id.empty())
            out.original_id = text(o, "name");
    }
    return out;
}

} // namespace oma::importers::otio
