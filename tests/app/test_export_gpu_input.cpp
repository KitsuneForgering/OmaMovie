// GPU-native encoder input (proposed ADR-0023): frames written on the GPU into the VA-API
// encoder's own surfaces must give the movie the readback route gives, byte for byte, and a
// device lost mid-export must still finish with the right frames.
#include "../playback/playback_test.hpp"
#include "decoded_frames.hpp"
#include "exporter.hpp"
#include "frame_match.hpp"

#include "oma/gpu/device.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

#include "oma_test.hpp"

namespace {
namespace pt = playback_test;

constexpr std::int64_t kFrames = 30;

// The camera fixture exported with the hardware encoder, through `gpu_input` or not; `lose_at`
// marks the device lost before that frame (-1: never).
QString export_camera(const QTemporaryDir& dir, const char* name, oma::gpu::Device& device,
                      bool gpu_input, std::int64_t lose_at, ExportStats& stats) {
    pt::Sequence seq = pt::make_sequence();
    if (!pt::place_camera(seq, 0).valid())
        return {};
    const QString movie = dir.filePath(name);
    const ExportRequest request{.timeline = seq.editor.timeline(),
                                .paths = pt::paths(),
                                .luts = {},
                                .file = movie.toStdString(),
                                .width = 320,
                                .height = 180,
                                .frames = kFrames,
                                .ticks_per_frame = 1600,
                                .audio_rate = pt::rate(),
                                .encoder = ExportRequest::Encoder::Hardware,
                                .gpu_input = gpu_input};
    const auto lose = [&](std::int64_t done, std::int64_t) {
        if (done == lose_at)
            device.mark_lost();
        return true;
    };
    return export_timeline_on(request, &device, lose, &stats) ? movie : QString();
}

QByteArray contents(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool matches_the_readback_route() {
    auto device = oma::gpu::Device::create();
    if (!device) {
        std::printf("    (skipped: no Vulkan device)\n");
        return true;
    }
    QTemporaryDir dir;
    ExportStats native;
    ExportStats readback;
    const QString a = export_camera(dir, "native.mp4", **device, true, -1, native);
    const QString b = export_camera(dir, "readback.mp4", **device, false, -1, readback);
    if (a.isEmpty() || b.isEmpty())
        return false;
    if (!native.encoder.starts_with("VA-API") || native.surface_frames == 0) {
        std::printf("    (GPU-native input not used here: %s; %s)\n", native.encoder.c_str(),
                    native.encoder_input.c_str());
        return native.surface_frames == 0 && contents(a) == contents(b); // both the readback route
    }
    std::printf("    %llu frames written into encoder surfaces; input: %s\n",
                static_cast<unsigned long long>(native.surface_frames),
                native.encoder_input.c_str());
    return native.surface_frames == kFrames && readback.surface_frames == 0 &&
           native.encoder_input.starts_with("written on the GPU") && contents(a) == contents(b);
}

// Lost at frame 10 while writing surfaces: the CPU reference finishes, matching a CPU export.
bool survives_device_loss() {
    auto device = oma::gpu::Device::create();
    if (!device)
        return true;
    QTemporaryDir dir;
    ExportStats lost;
    const QString movie = export_camera(dir, "lost.mp4", **device, true, 10, lost);
    if (movie.isEmpty() || decoded_frames(movie) != kFrames)
        return false;
    if (lost.surface_frames == 0)
        return true; // no GPU-native input on this machine: covered by the device-loss test
    const QByteArray frames = gray_frames(movie);
    const QByteArray source = gray_frames(QString("tests/fixtures/generated/h264_30fps_aac.mp4"));
    for (int i = 0; i < kFrames; ++i)
        if (closest(source, frames, i) != i)
            return false;
    return lost.surface_frames == 10 && lost.encoder_input.contains("CPU frames after device loss");
}
} // namespace

void run_export_gpu_input_tests() {
    describe("Export with GPU-native encoder input", {
        it("writes the movie the readback route writes, byte for byte",
           { expect(matches_the_readback_route()).toBeTruthy(); });
        it("finishes on the CPU after a device loss mid-export",
           { expect(survives_device_loss()).toBeTruthy(); });
    });
}
