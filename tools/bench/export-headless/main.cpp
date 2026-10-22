// Headless export bench: the app's own exporter on a Vulkan device of its own, with no window or
// Qt event loop, so it runs where a session cannot present
// (Docs/Research/evidence/2026-10-21-export-audit-hang.md). The input is a video (exported whole
// as one clip) or an OmaMovie project without LUTs, as tools/bench/cross-editor gives it.
// Prints ExportStats' phases. Usage: export_bench input out [gpu-input] [software]
#include "exporter.hpp"

#include "oma/gpu/device.hpp"
#include "oma/media/probe.hpp"
#include "oma/project/document.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string_view>

namespace tl = oma::timeline;

namespace {

struct Input {
    std::optional<tl::Timeline> timeline;
    MediaPaths paths;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// One video file as one clip from its first frame to its last, at its own rate and size.
std::optional<Input> from_video(const char* file) {
    const auto info = oma::media::probe(file);
    if (!info || !info->best_video)
        return std::nullopt;
    const oma::media::StreamInfo& v = info->streams[static_cast<std::size_t>(*info->best_video)];
    const auto rate = v.video && v.video->frame_rate ? *v.video->frame_rate : oma::frame_rates::k30;
    tl::Editor ed(tl::Timeline::create(rate, oma::SampleRate::make(48000).value()).value());
    const tl::MediaId media{1};
    const auto start = v.start.value_or(oma::RationalTime::make(0, v.timebase).value());
    const std::int64_t frames = v.frame_count.value_or(0);
    const auto per_frame = ed.timeline().to_ticks(rate.frame_to_time(1));
    const tl::TrackId track = ed.new_track_id();
    if (!v.duration || frames <= 0 || !per_frame ||
        !ed.add_media({.id = media,
                       .start = start,
                       .duration = *v.duration,
                       .has_video = true,
                       .has_audio = info->best_audio.has_value(),
                       .still = false}) ||
        !ed.execute(tl::edit::add_track(track, tl::TrackKind::Video, "V1")) ||
        !ed.execute(tl::edit::append(track, ed.new_clip_id(),
                                     {.media = media,
                                      .source_in = start,
                                      .duration = ed.timeline().at(frames * *per_frame),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}})))
        return std::nullopt;
    return Input{.timeline = ed.timeline(),
                 .paths = {{media.value(), file}},
                 .width = static_cast<std::uint32_t>(v.video->width),
                 .height = static_cast<std::uint32_t>(v.video->height)};
}

std::optional<Input> from_project(const char* file) {
    auto doc = oma::project::load(file);
    if (!doc || !doc->timeline || !doc->luts.empty())
        return std::nullopt; // LUT tables are loaded by the app; the bench scenes have none
    Input in{.timeline = doc->timeline, .paths = {}, .width = doc->canvas_width, .height = doc->canvas_height};
    for (const oma::project::MediaRef& m : doc->media)
        in.paths.emplace(m.info.id.value(), m.path.string());
    return in;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: export_bench input out [gpu-input] [software]\n");
        return 2;
    }
    const std::string_view source = argv[1];
    const auto input = source.ends_with(".omamovie") ? from_project(argv[1]) : from_video(argv[1]);
    if (!input) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    const tl::Timeline& timeline = *input->timeline;
    const auto per_frame = timeline.to_ticks(timeline.frame_rate().frame_to_time(1));
    const std::int64_t end = timeline.duration().value();
    if (!per_frame || end <= 0)
        return 1;
    bool gpu_input = false;
    bool software = false;
    for (int i = 3; i < argc; ++i) {
        gpu_input = gpu_input || std::string_view(argv[i]) == "gpu-input";
        software = software || std::string_view(argv[i]) == "software";
    }
    auto device = oma::gpu::Device::create();
    const ExportRequest request{.timeline = timeline,
                                .paths = input->paths,
                                .luts = {},
                                .file = argv[2],
                                .width = input->width & ~1U,
                                .height = input->height & ~1U,
                                .frames = ((end - 1) / *per_frame) + 1, // as Session::lastFrame() + 1
                                .ticks_per_frame = *per_frame,
                                .audio_rate = oma::SampleRate::make(48000).value(),
                                .encoder = software ? ExportRequest::Encoder::Software : ExportRequest::Encoder::Auto,
                                .gpu_input = gpu_input};
    ExportStats st;
    const auto done = export_timeline_on(request, device ? device->get() : nullptr,
                                         [](std::int64_t, std::int64_t) { return true; }, &st);
    const double n = static_cast<double>(std::max<std::int64_t>(st.frames, 1));
    std::printf("export %s: %lld frames in %.0f ms; process CPU %.1f ms/frame\n"
                "  per frame: build %.2f (worker %.2f), render %.2f, wait for encoder %.2f, encode %.2f, mix %.2f ms\n"
                "  encoder: %s; input: %s (%llu surface frames); decode: %s\n",
                done ? "ok" : done.error().summary().c_str(), static_cast<long long>(st.frames), st.total,
                st.process_cpu_ms / n, st.build / n, st.build_worker / n, st.render / n, st.convert / n,
                st.encode / n, st.audio / n, st.encoder.c_str(), st.encoder_input.c_str(),
                static_cast<unsigned long long>(st.surface_frames), st.decode_path.c_str());
    std::printf("  transfers per frame: readback %.2f, host copy %.2f, writer row copy %.2f, VA-API upload %.2f ms\n",
                st.readback_ms / n, st.nv12_host_copy_ms / n, st.writer_transfers.nv12_copy_ms / n,
                st.writer_transfers.va_upload_ms / n);
    return done ? 0 : 1;
}
