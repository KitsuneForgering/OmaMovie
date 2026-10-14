#include "exporter.hpp"

#include "oma/base/log.hpp"
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/format.hpp"
#include "oma/media/video_writer.hpp"
#include "oma/playback/timeline_audio.hpp"

#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace {

// One frame on the GPU, encoded to BT.709 RGBA8 and read back for the encoder.
oma::Result<std::vector<std::uint8_t>> render_gpu(oma::compositor::VulkanCompositor& gpu,
                                                  const ViewerFrame& view,
                                                  std::span<const oma::compositor::LayerInput> in) {
    if (auto rendered = gpu.render(view.graph, in); !rendered)
        return std::unexpected(rendered.error());
    if (auto encoded = gpu.encode_display(oma::compositor::VulkanCompositor::Transfer::Bt709);
        !encoded)
        return std::unexpected(encoded.error());
    return gpu.read_display();
}

} // namespace

oma::Result<void> export_timeline(const ExportRequest& request,
                                  const std::function<bool(std::int64_t, std::int64_t)>& progress,
                                  ExportStats* stats) {
    // Declared before the export's compositor, so it outlives it (§8.1).
    std::unique_ptr<oma::gpu::Device> device;
    if (auto created = oma::gpu::Device::create())
        device = std::move(*created);
    return export_timeline_on(request, device.get(), progress, stats);
}

oma::Result<void>
export_timeline_on(const ExportRequest& request, oma::gpu::Device* device,
                   const std::function<bool(std::int64_t, std::int64_t)>& progress,
                   ExportStats* stats) {
    using Clock = std::chrono::steady_clock;
    ExportStats local;
    ExportStats& st = stats != nullptr ? *stats : local;
    const auto started = Clock::now();
    auto mark = started;
    const auto lap = [&mark](double& into) {
        const auto now = Clock::now();
        into += std::chrono::duration<double, std::milli>(now - mark).count();
        mark = now;
    };
    const oma::FrameRate rate = request.timeline.frame_rate();
    // Software decode: export does not compete with the viewer's hardware decoder, and every
    // frame needs a CPU readback for the software encoder anyway.
    FrameSource frames(nullptr);
    std::unique_ptr<oma::compositor::VulkanCompositor> gpu;
    if (device != nullptr) {
        if (auto compositor = oma::compositor::VulkanCompositor::create(*device))
            gpu = std::move(*compositor);
    }
    if (!gpu)
        oma::log_warn(oma::Category::Encode,
                      "export: no Vulkan compositor, using the CPU reference (slow)");
    // Encoder: the GPU's where validated (M7 gate on Intel, 2026-10-07), else libx264. Both keep
    // the requested resolution, rate, colour tags and audio; only speed and the encoder's own
    // quality differ.
    using Encoder = ExportRequest::Encoder;
    constexpr std::uint32_t kIntel = 0x8086;
    const bool validated = device != nullptr && device->info().vendor_id == kIntel;
    const bool h264 = request.profile == oma::media::OutputProfile::Mp4H264Aac ||
                      request.profile == oma::media::OutputProfile::MatroskaH264Aac;
    const bool try_hardware = h264 && (request.encoder == Encoder::Hardware ||
                                       (request.encoder == Encoder::Auto && validated));
    const oma::media::VideoWriterOptions base{
        .audio = oma::media::AudioTrack{.rate = request.audio_rate, .channels = 2},
        .encoder = oma::media::VideoEncoder::Software,
        .render_node = {},
        .profile = request.profile};
    oma::Result<std::unique_ptr<oma::media::VideoWriter>> writer = std::unexpected(
        oma::Error(oma::ErrorCode::Unsupported, oma::Category::Encode, "not tried"));
    if (try_hardware) {
        auto hw = base;
        hw.encoder = oma::media::VideoEncoder::VaApi;
        if (device != nullptr)
            hw.render_node = device->info().render_node;
        writer = oma::media::VideoWriter::create(request.file, static_cast<int>(request.width),
                                                 static_cast<int>(request.height), rate, hw);
        st.encoder =
            writer ? "VA-API H.264 (GPU)"
                   : "libx264 (software): the GPU encoder failed: " + writer.error().summary();
    }
    if (!writer) {
        writer = oma::media::VideoWriter::create(request.file, static_cast<int>(request.width),
                                                 static_cast<int>(request.height), rate, base);
        if (!writer)
            return std::unexpected(writer.error());
        if (st.encoder.empty())
            st.encoder = oma::media::export_format(request.profile).video_encoder;
    }
    oma::log_info(oma::Category::Encode, "export encoder: {}", st.encoder);
    oma::log_info(oma::Category::Encode, "export {}x{} {} frames to {}: {} compositor",
                  request.width, request.height, request.frames, request.file.string(),
                  gpu ? "Vulkan" : "CPU");
    oma::playback::TimelineAudio audio(request.timeline, request.paths, request.audio_rate, 2);
    const auto sample_tb = oma::Rational::make(1, request.audio_rate.hz()).value();
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(request.width) * request.height * 3);
    std::vector<float> sound;
    std::int64_t sample = 0;
    for (std::int64_t k = 0; k < request.frames; ++k) {
        if (!progress(k, request.frames)) {
            return oma::make_error(oma::ErrorCode::Cancelled, oma::Category::Encode,
                                   "export cancelled");
        }
        mark = Clock::now();
        auto view = build_viewer_frame(request.timeline, request.paths, request.luts, request.width,
                                       request.height, k, request.ticks_per_frame, frames);
        if (!view)
            return std::unexpected(view.error());
        lap(st.build);
        std::vector<oma::compositor::LayerInput> inputs;
        inputs.reserve((*view)->pictures.size());
        for (const Picture& p : (*view)->pictures) {
            inputs.push_back({.frame = p.frame.get(),
                              .color = p.color,
                              .rotation = p.rotation,
                              .sample_aspect = p.sample_aspect});
        }
        if (gpu) {
            // The GPU encodes BT.709 RGBA8 and the software encoder reads it back: one 8-bit
            // readback per frame, the documented exception for export until a hardware encoder
            // takes GPU frames (CLAUDE.md §7.2). Measured: a float readback plus CPU encoding
            // cost ~43 ms per 1080p frame (Research/evidence/2026-10-07-m7-export.txt).
            auto pixels = render_gpu(*gpu, **view, inputs);
            if (!pixels && pixels.error().code() == oma::ErrorCode::DeviceLost) {
                // A lost device takes no more work (ADR-0005). The CPU reference renders the
                // same graph within the compositors' agreed tolerance, so the export finishes,
                // slower, instead of discarding the frames already written.
                oma::log_warn(oma::Category::Encode,
                              "export: GPU device lost at frame {}, continuing on the CPU "
                              "reference (slow)",
                              k);
                gpu.reset();
            } else if (!pixels) {
                return std::unexpected(pixels.error());
            } else {
                lap(st.render);
                if (auto w = (*writer)->write_rgba(*pixels, static_cast<int>(request.width) * 4);
                    !w)
                    return w;
            }
        }
        if (!gpu) {
            auto image = oma::compositor::CpuCompositor{}.render((*view)->graph, inputs);
            if (!image)
                return std::unexpected(image.error());
            lap(st.render);
            encode_bt709(image->pixels.data(),
                         static_cast<std::size_t>(image->width) * image->height, rgb.data());
            lap(st.convert);
            if (auto w = (*writer)->write(rgb, static_cast<int>(request.width) * 3); !w)
                return w;
        }
        lap(st.encode);
        // Sound for this frame, sample-accurate: the samples from this frame's start to the
        // next one's (frame boundaries rarely fall on whole samples at 29.97 fps).
        const auto next = rate.frame_to_time(k + 1).rescaled(sample_tb, oma::Rounding::Floor);
        if (!next)
            return std::unexpected(next.error());
        const std::int64_t count = next->value() - sample;
        sound.assign(static_cast<std::size_t>(count) * 2, 0.0F);
        if (auto r = audio.render(sound, sample); !r)
            return std::unexpected(r.error());
        if (auto w = (*writer)->write_audio(sound); !w)
            return w;
        lap(st.audio);
        sample = next->value();
        ++st.frames;
    }
    (void)progress(request.frames, request.frames);
    auto finished = (*writer)->finish();
    st.total = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    return finished;
}
