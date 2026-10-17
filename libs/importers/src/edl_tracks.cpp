#include "edl_tracks.hpp"

#include <format>

namespace oma::importers::edl {

std::vector<std::string> tracks_of(std::string_view channels) {
    if (channels == "V")
        return {"V"};
    if (channels == "A" || channels == "A1")
        return {"A1"};
    if (channels == "AA")
        return {"A1", "A2"};
    if (channels == "B" || channels == "A/V")
        return {"V", "A1"};
    if (channels == "AA/V")
        return {"V", "A1", "A2"};
    if (channels.size() == 2 && channels[0] == 'A' && channels[1] >= '2' && channels[1] <= '9')
        return {std::string(channels)};
    return {};
}

ir::Opaque note_of(const Event& e, const std::string& line) {
    return {.format = "edl",
            .original_id = e.number,
            .schema = line.starts_with("M2") ? "M2" : "comment",
            .raw = line,
            .fields = {}};
}

ir::Opaque event_of(const Event& e) {
    return {.format = "edl",
            .original_id = e.number,
            .schema = e.kind,
            .raw =
                std::format("{} {} {} {} {}", e.number, e.reel, e.channels, e.kind, e.transition),
            .fields = {{"reel", e.reel}, {"channels", e.channels}}};
}

void issue(ir::ImportReport& r, std::string object, std::string reason, std::string impact) {
    r.issues.push_back({.object = std::move(object),
                        .reason = std::move(reason),
                        .level = ir::SupportLevel::Edits,
                        .impact = std::move(impact)});
}

void keep_unread(Imported& out, std::vector<std::string> lines) {
    for (std::string& line : lines) {
        issue(out.report, "header", "line not read: " + line, "kept in the import");
        out.timeline.unsupported.push_back({.format = "edl",
                                            .original_id = {},
                                            .schema = "line",
                                            .raw = std::move(line),
                                            .fields = {}});
    }
}

} // namespace oma::importers::edl
