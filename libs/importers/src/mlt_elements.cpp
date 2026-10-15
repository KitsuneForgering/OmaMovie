#include "mlt_detail.hpp"

namespace oma::importers::mlt {

std::string property(const xml::Element& e, std::string_view name) {
    for (const xml::Element& c : e.children)
        if (c.name == "property" && c.attribute("name") == name)
            return c.text;
    return {};
}

Tractor tractor_of(const xml::Element& e, const Document& doc) {
    Tractor t;
    const auto in = frames_of(e.attribute("in"), doc);
    const auto out = frames_of(e.attribute("out"), doc);
    t.length = in && out && *out >= *in ? *out - *in + 1 : 0;
    for (const xml::Element& c : e.children) {
        if (c.name == "track")
            t.tracks.push_back({.producer = std::string(c.attribute("producer")),
                                .audio = c.attribute("hide") == "video",
                                .in = frames_of(c.attribute("in"), doc).value_or(0),
                                .out = frames_of(c.attribute("out"), doc).value_or(-1)});
        else if (c.name == "transition" || c.name == "filter")
            t.transitions.push_back(opaque(c));
    }
    return t;
}

// What was understood of an element: its attributes and properties (raw XML is not kept).
ir::Opaque opaque(const xml::Element& e) {
    ir::Opaque o{.format = "mlt",
                 .original_id = std::string(e.attribute("id")),
                 .schema = property(e, "mlt_service"),
                 .raw = {},
                 .fields = e.attributes};
    if (o.schema.empty())
        o.schema = e.name;
    for (const xml::Element& c : e.children)
        if (c.name == "property")
            o.fields.emplace_back(std::string(c.attribute("name")), c.text);
    return o;
}

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact) {
    r.issues.push_back({.object = std::move(object),
                        .reason = std::move(reason),
                        .level = ir::SupportLevel::Edits,
                        .impact = std::move(impact)});
}

} // namespace oma::importers::mlt
