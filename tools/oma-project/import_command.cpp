// `oma-project import FILE [RATE]` (OTIO, EDL, MLT/Kdenlive, FCPXML): the import report (CLAUDE.md
// §16.4, §16.7) from the importer and converter the app uses. Media durations come from the file;
// files are checked, not probed.
#include "commands.hpp"

#include "oma/ingest/timeline_read.hpp"
#include "oma/project/from_ir.hpp"

#include <cstdio>

int run_import(const std::filesystem::path& file, oma::Rational edl_rate) {
    auto read = oma::ingest::read_timeline(file, edl_rate);
    if (!read) {
        std::fprintf(stderr, "%s\n", read.error().summary().c_str());
        return 1;
    }
    auto* imported = &read->imported;
    const auto& dir = read->dir;
    const auto resolve = [&](const oma::ir::MediaRef& m) -> std::optional<oma::project::MediaRef> {
        const auto path = oma::importers::otio_local_path(m.target, dir);
        std::error_code ec;
        if (!m.available || !std::filesystem::exists(path, ec))
            return std::nullopt;
        oma::project::MediaRef found;
        found.info = {.id = {},
                      .start = m.available->start(),
                      .duration = m.available->duration(),
                      .has_video = true,
                      .has_audio = true,
                      .still = false};
        found.path = path;
        found.name = path.filename().string();
        return found;
    };
    oma::ir::ImportReport& report = imported->report;
    auto doc = oma::project::from_ir(imported->timeline, resolve, report);
    std::printf("format: %s\nversion: %s\nlevel reached: %d\n", report.format.c_str(),
                report.version.c_str(), static_cast<int>(report.reached));
    std::printf("media found: %zu of %zu (not probed)\n", doc ? doc->media.size() : 0,
                imported->timeline.media.size());
    for (const auto& i : report.issues)
        std::printf("issue: %s: %s; %s\n", i.object.c_str(), i.reason.c_str(), i.impact.c_str());
    return doc ? 0 : 1;
}
