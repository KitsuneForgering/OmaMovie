#include "exporter.hpp"

#include "export_encoder.hpp"

#include "oma/base/log.hpp"
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/format.hpp"
#include "oma/media/video_writer.hpp"
#include "oma/playback/timeline_audio.hpp"

#include <chrono>
#include <ctime>
#include <optional>
#include <span>
#include <vector>

namespace {

// One frame on the GPU for the encoder: written into one of its surfaces where `surfaces` is
// given (ADR-0023), else read back: NV12 for VA-API (converted on the GPU, 1.5 bytes per pixel),
// else BT.709 RGBA8 for libx264's own conversion.
oma::Result<EncodedFrame> render_gpu(oma::compositor::VulkanCompositor& gpu,
                                     const ViewerFrame& view,
                                     std::span<const oma::compositor::LayerInput> in, bool nv12,
                                     oma::media::EncoderSurfaces* surfaces, ExportStats& stats) {
    for (const auto& input : in) {
        if (input.frame == nullptr || input.frame->on_gpu())
            continue;
        ++stats.software_inputs;
        for (int p = 0; p < input.frame->plane_count(); ++p)
            stats.software_input_bytes += input.frame->plane(p).size();
    }
    oma::compositor::VulkanCompositor::RenderTransferStats transfers;
    if (auto rendered = gpu.render(view.graph, in, &transfers); !rendered)
        return std::unexpected(rendered.error());
    stats.staged_inputs += transfers.staged_inputs;
    stats.staged_bytes += transfers.staged_bytes;
    stats.staging_ms += transfers.staging_ms;
    stats.staging_row_copy_ms += transfers.row_copy_ms;
    EncodedFrame out;
    if (surfaces != nullptr) {
        auto surface = surfaces->acquire();
        if (!surface)
            return std::unexpected(surface.error());
        if (auto written = gpu.write_nv12({.luma = surface->luma,
                                           .luma_view = surface->luma_view,
                                           .cbcr = surface->cbcr,
                                           .cbcr_view = surface->cbcr_view,
                                           .foreign = true});
            !written)
            return std::unexpected(written.error());
        ++stats.surface_frames;
        out.kind = EncodedFrame::Pixels::Surface;
        out.surface = std::move(*surface);
        return out;
    }
    if (nv12) {
        const auto start = std::chrono::steady_clock::now();
        oma::compositor::VulkanCompositor::Nv12ReadbackTimings timings;
        auto image = gpu.read_nv12(&timings);
        stats.readback_ms +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        if (!image)
            return std::unexpected(image.error());
        stats.nv12_record_ms += timings.record_ms;
        stats.nv12_submit_ms += timings.submit_ms;
        stats.nv12_fence_wait_ms += timings.fence_wait_ms;
        stats.nv12_gpu_convert_ms += timings.gpu_convert_ms;
        stats.nv12_gpu_timestamp_frames += timings.gpu_timestamp_available ? 1 : 0;
        stats.nv12_invalidate_ms += timings.invalidate_ms;
        stats.nv12_host_copy_ms += timings.host_copy_ms;
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
    const auto cpu_started = std::clock();
    // On every return, so a failed export (a full disk, say) still reports where its time went.
    struct Total {
        ExportStats& st;
        Clock::time_point started;
        std::clock_t cpu_started;
        ~Total() {
            st.total = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
            st.process_cpu_ms =
                1000.0 * static_cast<double>(std::clock() - cpu_started) / CLOCKS_PER_SEC;
        }
    } const total{.st = st, .started = started, .cpu_started = cpu_started};
    auto mark = started;
    const auto lap = [&mark](double& into) {
        const auto now = Clock::now();
        into += std::chrono::duration<double, std::milli>(now - mark).count();
        mark = now;
    };
    const oma::FrameRate rate = request.timeline.frame_rate();
    // Hardware decode keeps 4K source pictures off the CPU and out of the staging copies
    // (Docs/Research/evidence/2026-10-19-export-hardware-decode.md); the policy matches the
    // viewer's and the encoder's below. The export's own decoders do not share the viewer's.
    constexpr std::uint32_t kIntel = 0x8086;
    const bool validated = device != nullptr && device->info().vendor_id == kIntel;
    using Decode = ExportRequest::Decode;
    const bool hardware_decode =
        device != nullptr &&
        (request.decode == Decode::Hardware || (request.decode == Decode::Auto && validated));
    // Replaced by a software source if the device is lost: its pictures live on that device.
    std::optional<FrameSource> frames;
    frames.emplace(hardware_decode ? device : nullptr, &st.decoded);
    st.decode_path = hardware_decode     ? "hardware first, software per file if it cannot open"
                     : device == nullptr ? "software (no GPU device)"
                     : request.decode == Decode::Software
                         ? "software (chosen in Settings)"
                         : "software (hardware decode not validated on this GPU)";
    st.compositor_path = "CPU reference (no Vulkan device)";
    std::unique_ptr<oma::compositor::VulkanCompositor> gpu;
    if (device != nullptr) {
        if (auto compositor = oma::compositor::VulkanCompositor::create(*device)) {
            gpu = std::move(*compositor);
        } else {
            st.compositor_path =
                "CPU reference (Vulkan compositor failed: " + compositor.error().summary() + ")";
        }
    }
    if (gpu)
        st.compositor_path = hardware_decode
                                 ? "Vulkan (hardware frames imported, software frames staged)"
                                 : "Vulkan (software frames staged to GPU)";
    if (!gpu)
        oma::log_warn(oma::Category::Encode, "export: {}", st.compositor_path);
    // Encoder: the GPU's where validated (M7 gate on Intel, 2026-10-07), else libx264. Both keep
    // the requested resolution, rate, colour tags and audio; only speed and the encoder's own
    // quality differ.
    using Encoder = ExportRequest::Encoder;
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
    bool vaapi = false;
    if (try_hardware) {
        auto hw = base;
        hw.encoder = oma::media::VideoEncoder::VaApi;
        if (device != nullptr)
            hw.render_node = device->info().render_node;
        writer = oma::media::VideoWriter::create(request.file, static_cast<int>(request.width),
                                                 static_cast<int>(request.height), rate, hw);
        vaapi = writer.has_value();
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
    // GPU-native encoder input (proposed ADR-0023), only when asked for: the readback stays the
    // route otherwise and the fallback whenever the encoder's surfaces cannot be used.
    std::unique_ptr<oma::media::EncoderSurfaces> surfaces;
    // libx264 also takes NV12, copied into its frame on the CPU; only VA-API uploads it.
    // Without a Vulkan compositor the CPU's RGBA is converted by swscale (VideoWriter::write_rgba).
    st.encoder_input = !gpu                         ? "CPU-rendered RGBA, converted to YUV"
                       : !(*writer)->takes_nv12()   ? "readback"
                       : vaapi                      ? "NV12 readback, uploaded"
                                                    : "NV12 readback, copied into the encoder's frame";
    if (request.gpu_input && gpu && (*writer)->takes_nv12()) {
        if (auto made = (*writer)->surfaces(*device)) {
            surfaces = std::move(*made);
            st.encoder_input = "written on the GPU into the encoder's surfaces";
        } else {
            st.encoder_input += ": GPU-native input unavailable: " + made.error().summary();
        }
    }
    oma::log_info(oma::Category::Encode, "export encoder: {}; input: {}", st.encoder,
                  st.encoder_input);
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
    const auto build_at = [&](std::int64_t at) {
        return build_viewer_frame(request.timeline, request.paths, request.luts, request.width,
                                  request.height, at, request.ticks_per_frame, *frames);
    };
    // The next frame is built (timeline evaluation, decode and import) on a worker of its own
    // while this thread renders the current one; the two took about as long and ran in turn.
    // While a build is in flight only that worker uses `frames`: it is joined before `frames`
    // changes, and the pool, declared after everything a build reads, joins it before they go.
    std::optional<oma::Result<std::shared_ptr<ViewerFrame>>> ahead;
    oma::JobPool builder(1);
    oma::JobHandle building;
    const auto build_ahead = [&](std::int64_t at) {
        building = builder.submit("export-build", [&, at](oma::JobContext&) {
            const auto started_at = Clock::now();
            ahead.emplace(build_at(at));
            st.build_worker += std::chrono::duration<double, std::milli>(Clock::now() - started_at).count();
            return oma::Result<void>{};
        });
    };
    const auto settle = [&] { // no build in flight afterwards
        if (building.valid())
            (void)building.wait();
        building = {};
    };
    for (std::int64_t k = 0; k < request.frames; ++k) {
        if (!progress(k, request.frames)) {
            settle();
            return stop(
                oma::Error(oma::ErrorCode::Cancelled, oma::Category::Encode, "export cancelled"));
        }
        mark = Clock::now();
        const auto build = [&] { return build_at(k); };
        settle();
        auto view = ahead ? std::move(*ahead) : build(); // the first frame, or after a device loss
        ahead.reset();
        if (!view)
            return stop(view.error());
        if (k + 1 < request.frames)
            build_ahead(k + 1);
        lap(st.build);
        std::vector<oma::compositor::LayerInput> inputs;
        const auto collect = [&] {
            inputs.clear();
            for (const Picture& p : (*view)->pictures) {
                inputs.push_back({.frame = p.frame.get(),
                                  .color = p.color,
                                  .rotation = p.rotation,
                                  .sample_aspect = p.sample_aspect});
            }
        };
        collect();
        EncodedFrame out;
        if (gpu) {
            // The GPU encodes BT.709 RGBA8 and the encoder reads it back: one 8-bit readback per
            // frame, the documented exception for export until a hardware encoder takes GPU
            // frames (M12 transfer baseline), overlapped with encoding the previous frame.
            auto pixels =
                render_gpu(*gpu, **view, inputs, (*writer)->takes_nv12(), surfaces.get(), st);
            if (!pixels && surfaces && pixels.error().code() != oma::ErrorCode::DeviceLost) {
                // The surfaces failed but the device works: read this frame and the rest back.
                oma::log_warn(oma::Category::Encode, "export: encoder surfaces failed at frame {}: {}",
                              k, pixels.error().summary());
                st.encoder_input += "; NV12 readback after frame " + std::to_string(k) + ": " +
                                    pixels.error().summary();
                surfaces.reset();
                pixels = render_gpu(*gpu, **view, inputs, true, nullptr, st);
            }
            if (!pixels && pixels.error().code() == oma::ErrorCode::DeviceLost) {
                // A lost device takes no more work (ADR-0005). The CPU reference renders the
                // same graph within the compositors' agreed tolerance, so the export finishes,
                // slower, instead of discarding the frames already written.
                oma::log_warn(oma::Category::Encode,
                              "export: GPU device lost at frame {}, continuing on the CPU "
                              "reference (slow)",
                              k);
                st.compositor_path +=
                    "; CPU reference after device loss at frame " + std::to_string(k);
                if (surfaces) {
                    st.encoder_input += "; CPU frames after device loss";
                    surfaces.reset(); // their planes were imported into the lost device
                }
                gpu.reset();
                if (hardware_decode) {
                    // This frame's pictures were decoded onto the lost device; decode it again,
                    // and the next one too (its build in flight used the lost device).
                    settle();
                    ahead.reset();
                    frames.emplace(nullptr, &st.decoded);
                    st.decode_path += "; software after device loss at frame " + std::to_string(k);
                    view = build();
                    if (!view)
                        return stop(view.error());
                    collect();
                }
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
