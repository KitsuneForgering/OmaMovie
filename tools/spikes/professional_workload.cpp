// Three-source decode and composition stress diagnostic. It does not measure presentation,
// audio, export, RAW debayering or HDR tone mapping. See docs/spikes/professional-workload.md.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_decoder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

bool number(std::string_view s, int& out) {
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), out);
    return error == std::errc{} && end == s.data() + s.size() && out > 0;
}

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void summarize(const char* label, std::vector<double>& values) {
    std::ranges::sort(values);
    const auto at = [&](double fraction) {
        return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
    };
    std::printf("%s p50=%.2f p95=%.2f p99=%.2f max=%.2f ms\n", label, at(0.5), at(0.95), at(0.99),
                values.back());
}

double cpu_seconds(const timeval& time) {
    return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_usec) / 1'000'000.0;
}

} // namespace

int main(int argc, char** argv) {
    int width = 0;
    int height = 0;
    int fps = 0;
    int frames = 0;
    if (argc != 9 || !number(argv[1], width) || !number(argv[2], height) || !number(argv[3], fps) ||
        !number(argv[4], frames) || width > 8192 || height > 8192 || fps > 120 || frames > 3600 ||
        (width & 1) != 0 || (height & 1) != 0 ||
        (std::string_view(argv[5]) != "software" && std::string_view(argv[5]) != "hardware")) {
        std::fprintf(stderr,
                     "usage: %s WIDTH HEIGHT FPS FRAMES software|hardware A B C\n"
                     "limits: even dimensions up to 8192, fps 1..120, frames 1..3600\n",
                     argv[0]);
        return 2;
    }

    auto device = oma::gpu::Device::create();
    if (!device) {
        std::fprintf(stderr, "Vulkan device: %s\n", device.error().summary().c_str());
        return 1;
    }
    const bool hardware = std::string_view(argv[5]) == "hardware";
    oma::media::VideoDecoderOptions options;
    options.device = device->get();
    if (!hardware)
        options.paths = {oma::media::DecodePath::Software};
    else
        options.paths = {oma::media::DecodePath::VaapiToVulkan,
                         oma::media::DecodePath::VulkanVideo};

    std::array<std::unique_ptr<oma::media::VideoDecoder>, 3> decoders;
    for (int i = 0; i < 3; ++i) {
        auto decoder = oma::media::VideoDecoder::open(argv[i + 6], options);
        if (!decoder) {
            std::fprintf(stderr, "source %d: %s\n", i, decoder.error().summary().c_str());
            return 1;
        }
        decoders[i] = std::move(*decoder);
    }
    auto compositor = oma::compositor::VulkanCompositor::create(**device);
    if (!compositor) {
        std::fprintf(stderr, "compositor: %s\n", compositor.error().summary().c_str());
        return 1;
    }

    oma::compositor::RenderGraph graph;
    graph.width = static_cast<std::uint32_t>(width);
    graph.height = static_cast<std::uint32_t>(height);
    oma::compositor::Layer main;
    main.fit = oma::compositor::Fit::Fill;
    main.color.contrast = 0.15;
    main.looks.push_back({.kind = oma::compositor::FilterKind::Vignette, .amount = 0.4});
    oma::compositor::Layer second;
    second.input = 1;
    second.transform = {.offset_x = static_cast<double>(width) * 0.21,
                        .offset_y = -static_cast<double>(height) * 0.18,
                        .scale_x = 0.48,
                        .scale_y = 0.48,
                        .rotation = 3.0};
    second.opacity = 0.85F;
    second.sharpness = -0.15;
    oma::compositor::Layer third;
    third.input = 2;
    third.transform = {.offset_x = -static_cast<double>(width) * 0.23,
                       .offset_y = static_cast<double>(height) * 0.2,
                       .scale_x = 0.36,
                       .scale_y = 0.36,
                       .rotation = -5.0};
    third.opacity = 0.7F;
    third.blend = oma::compositor::BlendMode::Screen;
    third.sharpness = 0.2;
    graph.layers = {main, second, third};

    std::printf("device=%s output=%dx%d deadline=%.2f ms mode=%s frames=%d\n",
                (*device)->info().name.c_str(), width, height, 1000.0 / fps, argv[5], frames);
    std::array<std::optional<oma::media::VideoFrame>, 3> pictures;
    constexpr int warmup = 8;
    std::vector<double> decode_ms;
    std::vector<double> render_ms;
    std::vector<double> total_ms;
    decode_ms.reserve(static_cast<std::size_t>(frames));
    render_ms.reserve(static_cast<std::size_t>(frames));
    total_ms.reserve(static_cast<std::size_t>(frames));
    rusage before{};
    rusage after{};
    Clock::time_point measured_start{};
    for (int i = 0; i < frames + warmup; ++i) {
        if (i == warmup) {
            getrusage(RUSAGE_SELF, &before);
            measured_start = Clock::now();
        }
        const auto start = Clock::now();
        for (int j = 0; j < 3; ++j) {
            auto frame = decoders[j]->next();
            if (!frame || !*frame) {
                std::fprintf(stderr,
                             "source %d ended or failed at frame %d; provide at least %d frames\n",
                             j, i, frames + warmup);
                return 1;
            }
            pictures[j] = std::move(**frame);
            if (hardware && decoders[j]->path() == oma::media::DecodePath::Software) {
                std::fprintf(stderr, "source %d fell back to software at frame %d\n", j, i);
                return 1;
            }
        }
        const double decoded = elapsed_ms(start);
        const std::array<oma::compositor::LayerInput, 3> inputs{
            {{.frame = &*pictures[0], .color = decoders[0]->stream().video->color},
             {.frame = &*pictures[1], .color = decoders[1]->stream().video->color},
             {.frame = &*pictures[2], .color = decoders[2]->stream().video->color}}};
        const auto render_start = Clock::now();
        auto rendered = (*compositor)->render(graph, inputs);
        if (!rendered) {
            std::fprintf(stderr, "render failed at frame %d: %s\n", i,
                         rendered.error().summary().c_str());
            return 1;
        }
        if (i >= warmup) {
            const double rendered_ms = elapsed_ms(render_start);
            decode_ms.push_back(decoded);
            render_ms.push_back(rendered_ms);
            total_ms.push_back(decoded + rendered_ms);
        }
    }
    const double measured_wall_ms = elapsed_ms(measured_start);
    getrusage(RUSAGE_SELF, &after);
    for (int i = 0; i < 3; ++i) {
        const auto& stream = decoders[i]->stream();
        const auto& video = *stream.video;
        std::printf("source%d=%dx%d codec=%s depth=%d nominal-fps=%.3f path=%s\n", i, video.width,
                    video.height, stream.codec.c_str(), video.bit_depth,
                    video.frame_rate ? video.frame_rate->fps().to_double_approx() : 0.0,
                    std::string(oma::media::to_string(decoders[i]->path())).c_str());
    }
    summarize("decode", decode_ms);
    summarize("composite", render_ms);
    summarize("sequential-total", total_ms);
    const double deadline = 1000.0 / fps;
    const auto late =
        std::ranges::count_if(total_ms, [deadline](double ms) { return ms > deadline; });
    std::printf(
        "sequential-over-deadline=%td/%d throughput=%.1f fps cpu=%.1f ms/frame peak-rss=%ld KiB\n",
        late, frames, static_cast<double>(frames) * 1000.0 / measured_wall_ms,
        (cpu_seconds(after.ru_utime) + cpu_seconds(after.ru_stime) - cpu_seconds(before.ru_utime) -
         cpu_seconds(before.ru_stime)) *
            1000.0 / frames,
        after.ru_maxrss);
    return 0;
}
