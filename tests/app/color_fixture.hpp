#pragma once

#include "oma/ingest/media_entry.hpp"
#include "viewer_frame.hpp"

#include "oma/media/format.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <optional>

namespace tl = oma::timeline;

// The camera fixture as the only clip, read with `matrix` and `range`; the picture the compositor
// would get at frame 0.
inline std::optional<oma::media::ColorInfo> composited_color(tl::MediaColorMatrix matrix,
                                                             tl::MediaColorRange range) {
    const std::string file = "tests/fixtures/generated/h264_30fps_aac.mp4";
    auto probed = oma::media::FfmpegFormatBackend{}.inspect_input(file);
    auto entry = probed ? oma::ingest::media_entry(*probed, tl::MediaId{1}) : std::nullopt;
    auto created =
        tl::Timeline::create(oma::frame_rates::k30, oma::SampleRate::make(48000).value());
    if (!entry || !created)
        return std::nullopt;
    entry->info.color_matrix = matrix;
    entry->info.color_range = range;
    tl::Editor ed(std::move(*created));
    const tl::TrackId track = ed.new_track_id();
    const auto source = tl::edit::ClipSource{.media = entry->info.id,
                                             .source_in = entry->info.start,
                                             .duration = ed.timeline().at(1600 * 10),
                                             .time_map = {},
                                             .video = {},
                                             .audio = {}};
    if (!ed.add_media(entry->info) ||
        !ed.execute(tl::edit::add_track(track, tl::TrackKind::Video, "V1")) ||
        !ed.execute(tl::edit::overwrite(track, ed.new_clip_id(), ed.timeline().at(0), source)))
        return std::nullopt;
    FrameSource frames;
    auto built = build_viewer_frame(ed.timeline(), {{1, file}}, {}, 64, 36, 0, 1600, frames);
    return built && !(*built)->pictures.empty() ? std::optional((*built)->pictures[0].color)
                                                : std::nullopt;
}
