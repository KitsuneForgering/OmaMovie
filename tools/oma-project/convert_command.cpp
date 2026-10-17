// `oma-project convert FILE.otio|FILE.edl OUT.omamovie [RATE]` (CLAUDE.md §16.7): the same import
// as the app's Import OpenTimelineIO… (libs/ingest, media probed), saved as a native project. An
// existing output is refused, not replaced.
#include "commands.hpp"

#include "oma/ingest/timeline_import.hpp"
#include "oma/media/format.hpp"

#include <cstdio>

int run_convert(const std::filesystem::path& file, const std::filesystem::path& out,
                oma::Rational edl_rate) {
    std::error_code ec;
    if (std::filesystem::exists(out, ec)) {
        std::fprintf(stderr, "%s exists; not replacing it\n", out.c_str());
        return 2;
    }
    const oma::media::FfmpegFormatBackend probe;
    oma::ir::ImportReport report;
    auto doc = oma::ingest::import_timeline_file(file, probe, report, edl_rate);
    for (const auto& i : report.issues)
        std::printf("issue: %s: %s; %s\n", i.object.c_str(), i.reason.c_str(), i.impact.c_str());
    if (!doc) {
        std::fprintf(stderr, "%s\n", doc.error().summary().c_str());
        return 1;
    }
    if (auto saved = oma::project::save(*doc, out); !saved) {
        std::fprintf(stderr, "%s\n", saved.error().summary().c_str());
        return 1;
    }
    std::printf("level reached: %d\nmedia: %zu\nsaved: %s\n", static_cast<int>(report.reached),
                doc->media.size(), out.c_str());
    return 0;
}
