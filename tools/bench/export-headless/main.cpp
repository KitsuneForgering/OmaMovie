// Headless export bench: one video file as one clip, exported by the app's own exporter on a
// Vulkan device of its own, with no window or Qt event loop, so it runs where a session cannot
// present (Docs/Research/evidence/2026-10-21-export-audit-hang.md). Prints ExportStats' phases.
// Usage: export_bench input.mp4 out.mp4 [gpu-input]
#include "exporter.hpp"

#include "oma/gpu/device.hpp"
#include "oma/media/probe.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <cstdio>
#include <string_view>

namespace tl = oma::timeline;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: export_bench input out [gpu-input]\n");
        return 2;
    }
    const auto info = oma::media::probe(argv[1]);
    if (!info || !info->best_video) {
        std::fprintf(stderr, "cannot probe %s\n", argv[1]);
        return 1;
    }
    const oma::media::StreamInfo& v = info->streams[static_cast<std::size_t>(*info->best_video)];
    const auto rate = v.video && v.video->frame_rate ? *v.video->frame_rate : oma::frame_rates::k30;
    const auto audio = oma::SampleRate::make(48000).value();
    tl::Editor ed(tl::Timeline::create(rate, audio).value());
    const tl::MediaId media{1};
    const auto start = v.start.value_or(oma::RationalTime::make(0, v.timebase).value());
    if (!v.duration || !ed.add_media({.id = media,
                                      .start = start,
                                      .duration = *v.duration,
                                      .has_video = true,
                                      .has_audio = info->best_audio.has_value(),
                                      .still = false}))
        return 1;
    const tl::TrackId track = ed.new_track_id();
    const std::int64_t frames = v.frame_count.value_or(0);
    const auto per_frame = ed.timeline().to_ticks(rate.frame_to_time(1));
    if (frames <= 0 || !per_frame || !ed.execute(tl::edit::add_track(track, tl::TrackKind::Video, "V1")) ||
        !ed.execute(tl::edit::append(track, ed.new_clip_id(),
                                     {.media = media,
                                      .source_in = start,
                                      .duration = ed.timeline().at(frames * *per_frame),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}}))) {
        std::fprintf(stderr, "cannot place the file (frames %lld)\n", static_cast<long long>(frames));
        return 1;
    }
    auto device = oma::gpu::Device::create();
    const ExportRequest request{.timeline = ed.timeline(),
                                .paths = {{media.value(), argv[1]}},
                                .luts = {},
                                .file = argv[2],
                                .width = static_cast<std::uint32_t>(v.video->width) & ~1U,
                                .height = static_cast<std::uint32_t>(v.video->height) & ~1U,
                                .frames = frames,
                                .ticks_per_frame = *per_frame,
                                .audio_rate = audio,
                                .gpu_input = argc > 3 && std::string_view(argv[3]) == "gpu-input"};
    ExportStats st;
    const auto done = export_timeline_on(request, device ? device->get() : nullptr,
                                         [](std::int64_t, std::int64_t) { return true; }, &st);
    const double n = static_cast<double>(std::max<std::int64_t>(st.frames, 1));
    std::printf("export %s: %lld frames in %.0f ms; process CPU %.1f ms/frame\n"
                "  per frame: build %.2f, render %.2f, wait for encoder %.2f, encode %.2f, mix %.2f ms\n"
                "  encoder: %s; input: %s (%llu surface frames); decode: %s\n",
                done ? "ok" : done.error().summary().c_str(), static_cast<long long>(st.frames), st.total,
                st.process_cpu_ms / n, st.build / n, st.render / n, st.convert / n, st.encode / n, st.audio / n,
                st.encoder.c_str(), st.encoder_input.c_str(), static_cast<unsigned long long>(st.surface_frames),
                st.decode_path.c_str());
    return done ? 0 : 1;
}
