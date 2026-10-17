#include "fcpxml_detail.hpp"

#include "oma/project_ir/validate.hpp"

namespace oma::importers {

Result<Imported> import_fcpxml(std::string_view text) {
    if (text.size() > kMaxFcpxmlBytes)
        return make_error(ErrorCode::InvalidData, Category::Importer, "file larger than 64 MiB");
    auto root = xml::parse(
        text, {.max_bytes = kMaxFcpxmlBytes, .max_depth = 64, .max_elements = 1'000'000});
    if (!root)
        return std::unexpected(root.error());
    if (root->name != "fcpxml")
        return make_error(ErrorCode::InvalidData, Category::Importer, "not an FCPXML document");
    Imported out;
    out.report.format = "FCPXML";
    out.report.version = std::string(root->attribute("version"));
    fcpxml::Context c{
        .out = out, .frame = *Rational::make(1, 25), .formats = {}, .assets = {}, .media = {}};
    if (const auto* resources = fcpxml::child_named(*root, "resources"))
        fcpxml::index_resources(c, *resources);
    std::vector<const xml::Element*> found;
    fcpxml::projects(*root, found);
    const xml::Element* sequence =
        found.empty() ? nullptr : fcpxml::child_named(*found[0], "sequence");
    if (sequence) {
        out.timeline.name = std::string(found[0]->attribute("name"));
        if (const auto f = c.formats.find(std::string(sequence->attribute("format")));
            f != c.formats.end())
            c.frame = f->second;
        else
            fcpxml::issue(out.report, out.timeline.name, "sequence without a known format",
                          "read at 25 fps");
        if (const auto* spine = fcpxml::child_named(*sequence, "spine"))
            fcpxml::read_spine(c, *spine);
    }
    for (std::size_t i = 1; i < found.size(); ++i)
        fcpxml::issue(out.report, std::string(found[i]->attribute("name")),
                      "only the first project is imported", "left out");
    if (auto valid = ir::validate(out.timeline); !valid)
        return std::unexpected(valid.error());
    out.report.reached =
        out.timeline.tracks.empty() ? ir::SupportLevel::Recognized : ir::SupportLevel::Edits;
    return out;
}

} // namespace oma::importers
