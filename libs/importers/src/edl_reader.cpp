#include "edl_tracks.hpp"

#include "oma/project_ir/validate.hpp"

#include <algorithm>

namespace oma::importers {

namespace {
std::unexpected<Error> refused(std::string what) {
    return make_error(ErrorCode::InvalidData, Category::Importer, std::move(what), "edl");
}
} // namespace

Result<Imported> import_edl(std::string_view text, Rational rate) {
    if (text.size() > kMaxEdlBytes)
        return refused("file larger than 16 MiB");
    if (!rate.is_positive() || rate.num() > 1000 * rate.den())
        return refused("invalid frame rate");
    const std::int64_t fps = (rate.num() + (rate.den() / 2)) / rate.den(); // 30 for 29.97
    bool drop = false;
    Imported out;
    out.report.format = "CMX 3600 EDL";
    out.report.version = "CMX 3600";
    std::vector<std::string> unread;
    auto events = edl::parse_events(text, fps, drop, out.timeline.name, unread);
    if (!events)
        return std::unexpected(events.error());
    edl::keep_unread(out, std::move(unread));
    if (drop && rate.den() != 1001)
        edl::issue(out.report, "FCM", "drop-frame timecode at a rate that has none",
                   "read as drop-frame at the given rate");
    const auto tb = *rate.inverse();
    const std::int64_t start =
        events->empty() ? 0 : std::ranges::min(*events, {}, &edl::Event::rec_in).rec_in;
    out.timeline.global_start = *RationalTime::make(start, tb);
    edl::Build b{
        .out = out, .timebase = tb, .start = start, .tracks = {}, .cursor = {}, .media = {}};
    for (const edl::Event& e : *events) {
        const auto names = edl::tracks_of(e.channels);
        if (names.empty())
            edl::issue(out.report, e.number, std::format("channels {} not imported", e.channels),
                       "event left out");
        for (const std::string& name : names)
            edl::place(b, e, name);
    }
    if (auto v = b.tracks.find("V"); v != b.tracks.end()) { // the picture first: the storyline
        out.timeline.tracks.push_back(std::move(v->second));
        b.tracks.erase(v);
    }
    for (auto& [name, track] : b.tracks)
        out.timeline.tracks.push_back(std::move(track));
    if (auto valid = ir::validate(out.timeline); !valid)
        return std::unexpected(valid.error());
    out.report.reached =
        out.timeline.tracks.empty() ? ir::SupportLevel::Recognized : ir::SupportLevel::Edits;
    return out;
}

} // namespace oma::importers
