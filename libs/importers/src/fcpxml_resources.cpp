#include "fcpxml_detail.hpp"

namespace oma::importers::fcpxml {

void index_resources(Context& c, const xml::Element& resources) {
    for (const xml::Element& e : resources.children) {
        const std::string id(e.attribute("id"));
        if (e.name == "format") {
            if (const auto frame = seconds(e.attribute("frameDuration"));
                frame && frame->is_positive())
                c.formats[id] = *frame;
        } else if (e.name == "asset") {
            Asset a{.name = std::string(e.attribute("name")),
                    .src = std::string(e.attribute("src")),
                    .start = std::string(e.attribute("start")),
                    .duration = std::string(e.attribute("duration"))};
            for (const xml::Element& rep : e.children) // 1.9+: <media-rep kind="original-media">
                if (rep.name == "media-rep" &&
                    (a.src.empty() || rep.attribute("kind") == "original-media"))
                    a.src = std::string(rep.attribute("src"));
            c.assets[id] = std::move(a);
        } else if (e.name == "media") { // a compound clip or multicam: not read yet
            c.out.timeline.unsupported.push_back(opaque(e));
            issue(c.out.report, id, "compound or multicam media not imported",
                  "kept in the import");
        } else if (e.name != "effect") {
            issue(c.out.report, id.empty() ? e.name : id, e.name + " resource not imported",
                  "left out");
        }
    }
}

ir::Opaque opaque(const xml::Element& e) {
    ir::Opaque o{.format = "fcpxml",
                 .original_id = std::string(e.attribute("name")),
                 .schema = e.name,
                 .raw = {},
                 .fields = e.attributes};
    if (o.original_id.empty())
        o.original_id = std::string(e.attribute("id"));
    return o;
}

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact) {
    r.issues.push_back({.object = std::move(object),
                        .reason = std::move(reason),
                        .level = ir::SupportLevel::Edits,
                        .impact = std::move(impact)});
}

} // namespace oma::importers::fcpxml
