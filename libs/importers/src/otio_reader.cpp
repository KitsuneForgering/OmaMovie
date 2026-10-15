#include "otio_detail.hpp"

#include "oma/project_ir/validate.hpp"

#include <format>

namespace oma::importers {

namespace {
constexpr std::size_t kMaxBytes = std::size_t{64} << 20;
constexpr std::size_t kMaxDepth = 64;

std::unexpected<Error> refused(std::string what) {
    return make_error(ErrorCode::InvalidData, Category::Importer, std::move(what), "otio");
}

} // namespace

Result<Imported> import_otio(std::string_view json) {
    if (json.size() > kMaxBytes)
        return refused("file larger than 64 MiB");
    simdjson::dom::parser parser;
    otio::Element root;
    otio::Object top;
    if (parser.allocate(json.size(), kMaxDepth) != simdjson::SUCCESS ||
        parser.parse(simdjson::padded_string(json)).get(root) != simdjson::SUCCESS ||
        root.get(top) != simdjson::SUCCESS)
        return refused("not JSON, or nested deeper than 64 levels");
    Imported out{.timeline = {},
                 .report = {.format = "OpenTimelineIO JSON",
                            .version = otio::schema_of(top),
                            .reached = ir::SupportLevel::Recognized,
                            .issues = {}}};
    if (!out.report.version.starts_with("Timeline."))
        return refused(std::format("not an OTIO timeline ({})", out.report.version));
    otio::Context c{.report = out.report, .items = 0, .media_by_target = {}};
    out.timeline.name = otio::text(top, "name");
    otio::Element start;
    if (top["global_start_time"].get(start) == simdjson::SUCCESS && !start.is_null()) {
        auto t = otio::time_of(start, c, "global_start_time");
        if (!t)
            return std::unexpected(t.error());
        out.timeline.global_start = *t;
    }
    if (auto read = otio::read_stack(top, out, c); !read)
        return std::unexpected(read.error());
    return out;
}

} // namespace oma::importers
