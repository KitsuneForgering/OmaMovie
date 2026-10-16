#include "exporter.hpp"

#include "export_encoder.hpp"

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

// One frame on the GPU, read back for the encoder: NV12 for VA-API (converted on the GPU, 1.5
// bytes per pixel), else BT.709 RGBA8 for libx264's own conversion.
oma::Result<EncodedFrame> render_gpu(oma::compositor::VulkanCompositor& gpu,
                                     const ViewerFrame& view,
                                     std::span<const oma::compositor::LayerInput> in, bool nv12,
                                     ExportStats& stats) {
    for (const auto& input : in) {
        if (input.frame == nullptr || input.frame->on_gpu())
            continue;
        ++stats.software_inputs;
        for (int p = 0; p < input.frame->plane_count(); ++p)
            stats.software_input_bytes += input.frame->plane(p).size();
    }
    if (auto rendered = gpu.render(view.graph, in); !rendered)
        return std::unexpected(rendered.error());
    EncodedFrame out;
    if (nv12) {
        const auto start = std::chrono::steady_clock::now();
        auto image = gpu.read_nv12();
        stats.readback_ms +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        if (!image)
            return std::unexpected(image.error());
        ++stats.readbacks;
        stats.readback_bytes += image->bytes.size();
        out.kind = EncodedFrame::Pixels::Nv12;
        out.stride = static_cast<int>(image->stride);
        out.pixels = std::move(image->bytes);
        return out;
    }
    if (auto encoded = gpu.encode_display(oma::compositor::VulkanCompositor::Transfer::Bt709);
        !encoded)
        return std::unexpected(encoded.error());
    const auto start = std::chrono::steady_clock::now();
    auto pixels = gpu.read_display();
    stats.readback_ms +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!pixels)
        return std::unexpected(pixels.error());
    ++stats.readbacks;
    stats.readback_bytes += pixels->size();
    out.kind = EncodedFrame::Pixels::Rgba;
    out.pixels = std::move(*pixels);
    return out;
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
    // On every return, so a failed export (a full disk, say) still reports where its time went.
    struct Total {
        ExportStats& st;
        Clock::time_point started;
        ~Total() {
            st.total = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        }
    } const total{.st = st, .started = started};
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
    std::int64_t sample = 0;
    // Declared after the writer: destroyed first, joining its worker before the writer goes.
    EncodeStage encoder(**writer, static_cast<int>(request.width));
    const auto stop = [&](oma::Error error) -> oma::Result<void> {
        (void)encoder.finish(); // joins the worker before the writer is touched again
        st.encode = encoder.encode_ms();
        st.writer_transfers = (*writer)->transfer_stats();
        return std::unexpected(std::move(error));
    };
    for (std::int64_t k = 0; k < request.frames; ++k) {
        if (!progress(k, request.frames)) {
            return stop(
                oma::Error(oma::ErrorCode::Cancelled, oma::Category::Encode, "export cancelled"));
        }
        mark = Clock::now();
        auto view = build_viewer_frame(request.timeline, request.paths, request.luts, request.width,
                                       request.height, k, request.ticks_per_frame, frames);
        if (!view)
            return stop(view.error());
        lap(st.build);
        std::vector<oma::compositor::LayerInput> inputs;
        inputs.reserve((*view)->pictures.size());
        for (const Picture& p : (*view)->pictures) {
            inputs.push_back({.frame = p.frame.get(),
                              .color = p.color,
                              .rotation = p.rotation,
                              .sample_aspect = p.sample_aspect});
        }
        EncodedFrame out;
        if (gpu) {
            // The GPU encodes BT.709 RGBA8 and the encoder reads it back: one 8-bit readback per
            // frame, the documented exception for export until a hardware encoder takes GPU
            // frames (M12 transfer baseline), overlapped with encoding the previous frame.
            auto pixels = render_gpu(*gpu, **view, inputs, (*writer)->takes_nv12(), st);
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
                return stop(pixels.error());
            } else {
                out = std::move(*pixels);
            }
        }
        if (!gpu) {
            auto image = oma::compositor::CpuCompositor{}.render((*view)->graph, inputs);
            if (!image)
                return stop(image.error());
            out.pixels.resize(static_cast<std::size_t>(request.width) * request.height * 3);
            encode_bt709(image->pixels.data(),
                         static_cast<std::size_t>(image->width) * image->height, out.pixels.data());
        }
        lap(st.render);
        // Sound for this frame, sample-accurate: the samples from this frame's start to the
        // next one's (frame boundaries rarely fall on whole samples at 29.97 fps).
        const auto next = rate.frame_to_time(k + 1).rescaled(sample_tb, oma::Rounding::Floor);
        if (!next)
            return stop(next.error());
        out.sound.assign(static_cast<std::size_t>(next->value() - sample) * 2, 0.0F);
        if (auto r = audio.render(out.sound, sample); !r)
            return stop(r.error());
        lap(st.audio);
        if (!encoder.push(std::move(out)))
            break;       // the encoder failed: finish() below returns why
        lap(st.convert); // time spent waiting for the encoder (backpressure)
        sample = next->value();
        ++st.frames;
    }
    const auto encoded = encoder.finish();
    st.encode = encoder.encode_ms();
    st.writer_transfers = (*writer)->transfer_stats();
    if (!encoded)
        return encoded;
    (void)progress(request.frames, request.frames);
    return (*writer)->finish();
}
