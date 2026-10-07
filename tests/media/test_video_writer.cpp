#include "oma/media/audio_decoder.hpp"
#include "oma/media/format.hpp"
#include "oma/media/video_decoder.hpp"
#include "oma/media/video_writer.hpp"

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <random>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

#include "oma_test.hpp"

namespace {

void writes_and_reopens_video() {
    namespace fs = std::filesystem;
    using namespace oma::media;
    if (!FfmpegFormatBackend{}.check_output({"mp4", "libx264", std::nullopt})) {
        std::puts("    (skipping H.264 output: libx264 unavailable)");
        return;
    }
    const fs::path file =
        fs::temp_directory_path() / ("oma-video-writer-" + std::to_string(::getpid()) + ".mp4");
    {
        std::ofstream(file) << "previous output";
        auto abandoned = VideoWriter::create(file, 32, 32, oma::frame_rates::k29_97);
        expect(abandoned.has_value()).toBeTruthy();
    }
    expect(fs::file_size(file)).toBe(15U); // unfinished output kept the old file

    auto writer = VideoWriter::create(file, 32, 32, oma::frame_rates::k29_97);
    expect(writer.has_value()).toBeTruthy();
    if (!writer)
        return;
    std::vector<std::uint8_t> rgb(32U * 32U * 3U);
    expect(!(*writer)->write(rgb, 31 * 3)).toBeTruthy();
    for (int i = 0; i < 3; ++i) {
        std::ranges::fill(rgb, 0);
        for (std::size_t p = 0; p < rgb.size(); p += 3)
            rgb[p + static_cast<std::size_t>(i)] = 255;
        expect((*writer)->write(rgb, 32 * 3).has_value()).toBeTruthy();
    }
    expect((*writer)->finish().has_value()).toBeTruthy();
    expect(!(*writer)->finish()).toBeTruthy();
    expect(fs::file_size(file) > 15U).toBeTruthy();

    VideoDecoderOptions options;
    options.paths = {DecodePath::Software};
    auto decoder = VideoDecoder::open(file, options);
    expect(decoder.has_value()).toBeTruthy();
    if (decoder) {
        const auto& color = (*decoder)->stream().video->color;
        expect(color.matrix == 1 && color.primaries == 1 && color.transfer == 1 &&
               color.range == ColorRange::Limited)
            .toBeTruthy();
        int count = 0;
        for (;;) {
            auto frame = (*decoder)->next();
            expect(frame.has_value()).toBeTruthy();
            if (!frame || !*frame)
                break;
            const auto pts = (*frame)->pts();
            expect(pts.has_value()).toBeTruthy();
            if (pts) {
                auto in_rate = pts->rescaled(oma::frame_rates::k29_97.frame_duration(),
                                             oma::Rounding::Nearest);
                expect(in_rate && in_rate->value() == count).toBeTruthy();
            }
            std::vector<std::uint8_t> rgba(32U * 32U * 4U);
            expect((*frame)->copy_rgba(rgba, 32 * 4).has_value()).toBeTruthy();
            if (count < 3) {
                const auto pixel = rgba.data() + ((16 * 32 + 16) * 4);
                expect(pixel[count] > 180 && pixel[(count + 1) % 3] < 80 &&
                       pixel[(count + 2) % 3] < 80)
                    .toBeTruthy();
            }
            ++count;
        }
        expect(count).toBe(3);
    }
    fs::remove(file);
}

// One second of 30 fps video with one second of 48 kHz stereo sine: the file holds both, and
// the sound decodes back with its length (AAC adds at most one frame of padding) and level.
void writes_audio_with_video() {
    namespace fs = std::filesystem;
    using namespace oma::media;
    if (!FfmpegFormatBackend{}.check_output({"mp4", "libx264", std::nullopt})) {
        std::puts("    (skipping H.264 output: libx264 unavailable)");
        return;
    }
    const fs::path file =
        fs::temp_directory_path() / ("oma-video-audio-" + std::to_string(::getpid()) + ".mp4");
    const AudioTrack track{.rate = oma::SampleRate::make(48000).value(), .channels = 2};
    auto writer =
        VideoWriter::create(file, 32, 32, oma::frame_rates::k30,
                            {.audio = track, .encoder = VideoEncoder::Software, .render_node = {}});
    expect(writer.has_value()).toBeTruthy();
    if (!writer)
        return;
    std::vector<std::uint8_t> rgba(32U * 32U * 4U, 0); // RGBA input, as a GPU readback gives it
    for (std::size_t p = 0; p < rgba.size(); p += 4)
        rgba[p] = 200;                    // red; alpha 0 must not matter
    std::vector<float> block(1600U * 2U); // one video frame of sound
    for (int frame = 0; frame < 30; ++frame) {
        for (std::size_t i = 0; i < 1600; ++i) {
            const double t = static_cast<double>((frame * 1600) + static_cast<int>(i)) / 48000.0;
            const auto v = static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * t));
            block[2 * i] = v;
            block[(2 * i) + 1] = v;
        }
        expect((*writer)->write_rgba(rgba, 32 * 4).has_value()).toBeTruthy();
        expect((*writer)->write_audio(block).has_value()).toBeTruthy();
    }
    expect((*writer)->finish().has_value()).toBeTruthy();
    // A finished movie is readable like any file, not private like its temporary.
    expect((fs::status(file).permissions() & fs::perms::others_read) != fs::perms::none)
        .toBeTruthy();
    auto decoder = AudioDecoder::open(file);
    expect(decoder.has_value()).toBeTruthy();
    std::int64_t frames = 0;
    double energy = 0.0;
    std::int64_t counted = 0;
    while (decoder) {
        auto buffer = (*decoder)->next();
        if (!buffer || !*buffer)
            break;
        for (std::int64_t i = 0; i < (*buffer)->frames; ++i, ++frames) {
            if (frames >= 4800 && frames < 43200) { // away from the encoder's edges
                const double v = (*buffer)->channel(0)[static_cast<std::size_t>(i)];
                energy += v * v;
                ++counted;
            }
        }
    }
    expect(frames >= 48000 && frames <= 48000 + 1024).toBeTruthy();
    const double rms = counted > 0 ? std::sqrt(energy / static_cast<double>(counted)) : 0.0;
    expect(rms).toBeCloseTo(0.5 / std::numbers::sqrt2, 0.02);
    VideoDecoderOptions video_options;
    video_options.paths = {DecodePath::Software};
    auto video = VideoDecoder::open(file, video_options);
    auto first = video ? (*video)->next() : decltype((*video)->next()){};
    expect(first && *first).toBeTruthy();
    if (first && *first) {
        std::vector<std::uint8_t> back(32U * 32U * 4U);
        expect((*first)->copy_rgba(back, 32 * 4).has_value()).toBeTruthy();
        const auto* pixel = back.data() + ((16 * 32 + 16) * 4);
        expect(std::abs(pixel[0] - 200) <= 6 && pixel[1] < 12 && pixel[2] < 12).toBeTruthy();
    }
    fs::remove(file);
}

// The GPU's H.264 encoder through VA-API (skipped where the machine has none): same frames,
// rate and colour tags as the software path, a picture close to what was written.
void writes_with_vaapi() {
    namespace fs = std::filesystem;
    using namespace oma::media;
    const fs::path file =
        fs::temp_directory_path() / ("oma-video-vaapi-" + std::to_string(::getpid()) + ".mp4");
    auto writer = VideoWriter::create(
        file, 64, 64, oma::frame_rates::k30,
        {.audio = std::nullopt, .encoder = VideoEncoder::VaApi, .render_node = {}});
    if (!writer) {
        std::printf("    (skipping VA-API output: %s)\n", writer.error().summary().c_str());
        return;
    }
    std::vector<std::uint8_t> rgba(64U * 64U * 4U, 255);
    for (std::size_t p = 0; p < rgba.size(); p += 4) {
        rgba[p + 1] = 40; // orange-red
        rgba[p + 2] = 0;
    }
    for (int i = 0; i < 30; ++i)
        expect((*writer)->write_rgba(rgba, 64 * 4).has_value()).toBeTruthy();
    expect((*writer)->finish().has_value()).toBeTruthy();
    VideoDecoderOptions options;
    options.paths = {DecodePath::Software};
    auto decoder = VideoDecoder::open(file, options);
    expect(decoder.has_value()).toBeTruthy();
    int count = 0;
    while (decoder) {
        auto frame = (*decoder)->next();
        if (!frame || !*frame)
            break;
        if (count == 15) {
            const auto& color = (*decoder)->stream().video->color;
            expect(color.matrix == 1 && color.primaries == 1 && color.transfer == 1).toBeTruthy();
            std::vector<std::uint8_t> back(64U * 64U * 4U);
            expect((*frame)->copy_rgba(back, 64 * 4).has_value()).toBeTruthy();
            const auto* pixel = back.data() + ((32 * 64 + 32) * 4);
            expect(std::abs(pixel[0] - 255) <= 8 && std::abs(pixel[1] - 40) <= 8 && pixel[2] < 12)
                .toBeTruthy();
        }
        ++count;
    }
    expect(count).toBe(30);
    fs::remove(file);
}

// Disk full, by failure injection: a child process may write at most 64 KiB per file
// (RLIMIT_FSIZE; writes past it fail with EFBIG once SIGXFSZ is ignored). The export must fail
// with an error, never crash, leave the previous file untouched and remove its temporary.
void survives_a_full_disk() {
    namespace fs = std::filesystem;
    using namespace oma::media;
    if (!FfmpegFormatBackend{}.check_output({"mp4", "libx264", std::nullopt})) {
        std::puts("    (skipping H.264 output: libx264 unavailable)");
        return;
    }
    const fs::path dir =
        fs::temp_directory_path() / ("oma-video-full-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    const fs::path file = dir / "movie.mp4";
    std::ofstream(file) << "previous output";
    const pid_t child = ::fork();
    if (child == 0) {
        std::signal(SIGXFSZ, SIG_IGN);
        const rlimit limit{.rlim_cur = 64 * 1024, .rlim_max = 64 * 1024};
        ::setrlimit(RLIMIT_FSIZE, &limit);
        auto writer = VideoWriter::create(file, 320, 180, oma::frame_rates::k30);
        if (!writer)
            ::_exit(3);
        std::mt19937 noise(7);
        std::vector<std::uint8_t> rgb(320U * 180U * 3U);
        bool failed = false;
        for (int i = 0; i < 120 && !failed; ++i) {
            for (auto& v : rgb)
                v = static_cast<std::uint8_t>(noise());
            failed = !(*writer)->write(rgb, 320 * 3);
        }
        failed = failed || !(*writer)->finish();
        writer->reset(); // removes the temporary
        ::_exit(failed ? 0 : 2);
    }
    int status = 0;
    ::waitpid(child, &status, 0);
    expect(WIFEXITED(status) && WEXITSTATUS(status) == 0).toBeTruthy(); // an error, not a crash
    expect(fs::file_size(file)).toBe(15U);                              // the old file stays
    std::size_t leftovers = 0;
    for (const auto& e : fs::directory_iterator(dir))
        leftovers += e.path() != file ? 1U : 0U;
    expect(leftovers).toBe(0U);
    fs::remove_all(dir);
}

} // namespace

void run_video_writer_tests() {
    describe("media video writer", {
        it("video writer replaces output after writing rational-rate frames",
           { writes_and_reopens_video(); });
        it("adds an AAC track from interleaved float samples", { writes_audio_with_video(); });
        it("encodes H.264 on the GPU through VA-API", { writes_with_vaapi(); });
        it("fails cleanly when the disk fills, keeping the old file", { survives_a_full_disk(); });
    });
}
