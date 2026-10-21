#include "mlt_detail.hpp"

#include "oma/project_ir/validate.hpp"

#include <algorithm>

namespace oma::importers {

Result<Imported> import_mlt(std::string_view text) {
    if (text.size() > kMaxMltBytes)
        return make_error(ErrorCode::InvalidData, Category::Importer, "file larger than 64 MiB");
    auto root =
        xml::parse(text, {.max_bytes = kMaxMltBytes, .max_depth = 64, .max_elements = 1'000'000});
    if (!root)
        return std::unexpected(root.error());
    if (root->name != "mlt")
        return make_error(ErrorCode::InvalidData, Category::Importer, "not an MLT document");
    Imported out;
    out.report.format = "MLT XML";
    out.report.version = std::string(root->attribute("version"));
    if (!mlt::property(*root, "kdenlive:docproperties.version").empty() ||
        root->attribute("producer").starts_with("main_bin"))
        out.report.format = "Kdenlive (MLT XML)";
    out.timeline.name = std::string(root->attribute("title"));
    auto doc = mlt::index(*root, out.report);
    if (!doc)
        return std::unexpected(doc.error());
    if (auto built = mlt::build(*doc, out); !built)
        return std::unexpected(built.error());
    for (auto& track : out.timeline.tracks)
        for (auto& item : track.items)
            if (const auto* c = std::get_if<ir::Clip>(&item)) {
                const auto remaps = std::ranges::count_if(c->unsupported, [](const ir::Opaque& o) {
                    return mlt::is_a(o, "link") && o.schema == "timeremap";
                });
                const auto inert = std::ranges::count_if(c->unsupported, mlt::inert);
                if (std::ssize(c->unsupported) > remaps + inert)
                    mlt::issue(out.report, c->name, "filters not imported", "kept in the import");
                if (remaps > 0)
                    mlt::issue(out.report, c->name, "time remap not imported",
                               "plays at normal speed for its length");
            }
    if (auto valid = ir::validate(out.timeline); !valid)
        return std::unexpected(valid.error());
    out.report.reached =
        out.timeline.tracks.empty() ? ir::SupportLevel::Recognized : ir::SupportLevel::Edits;
    return out;
}

} // namespace oma::importers
