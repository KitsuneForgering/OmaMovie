#include "oma/ingest/timeline_import.hpp"

#include "oma/ingest/media_entry.hpp"
#include "oma/ingest/timeline_read.hpp"
#include "oma/project/from_ir.hpp"

#include <algorithm>
#include <cstdint>

namespace oma::ingest {

Result<project::Document> import_timeline_file(const std::filesystem::path& path,
                                               const media::MediaImporter& importer,
                                               ir::ImportReport& report, Rational edl_rate) {
    auto read = read_timeline(path, edl_rate);
    if (!read) {
        return std::unexpected(read.error());
    }
    auto& imported = read->imported;
    report = std::move(imported.report);
    const auto& dir = read->dir;
    // The canvas follows the first picture found, as a library import does (Session), so a
    // 320x180 timeline does not open on a 1920x1080 canvas.
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const auto resolve = [&](const ir::MediaRef& m) -> std::optional<project::MediaRef> {
        const auto file = importers::otio_local_path(m.target, dir);
        auto probed = importer.inspect_input(file);
        auto entry = probed ? media_entry(*probed, {}) : std::nullopt;
        if (!entry) {
            return std::nullopt;
        }
        if (width == 0 && entry->width > 0 && entry->height > 0) {
            width = entry->width;
            height = entry->height;
        }
        return project::MediaRef{
            .info = entry->info,
            .path = file,
            .name = file.filename().string(),
            .audio_only = entry->audio_only,
            .fingerprint = project::fingerprint_file(file).value_or(project::Fingerprint{})};
    };
    auto doc = project::from_ir(imported.timeline, resolve, report);
    if (doc && width > 0) {
        // Even sizes keep 4:2:0 exports possible, within the canvas limits (ADR-0010).
        doc->canvas_width = std::clamp<std::uint32_t>((width + 1) & ~1U, 16, 16384);
        doc->canvas_height = std::clamp<std::uint32_t>((height + 1) & ~1U, 16, 16384);
    }
    return doc;
}

} // namespace oma::ingest
