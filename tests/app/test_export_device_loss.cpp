#include "../playback/playback_test.hpp"
#include "decoded_frames.hpp"
#include "exporter.hpp"
#include "frame_match.hpp"

#include "oma/gpu/device.hpp"

#include <QTemporaryDir>

#include <cstdio>

#include "oma_test.hpp"

namespace {
namespace pt = playback_test;

constexpr std::int64_t kFrames = 30; // the whole camera fixture
constexpr std::int64_t kLostAt = 10;

QString export_camera(const QTemporaryDir& dir, const char* name, oma::gpu::Device* device,
                      std::int64_t lose_at, ExportStats* stats = nullptr) {
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
                                .encoder = ExportRequest::Encoder::Software};
    const auto lose = [&](std::int64_t done, std::int64_t) {
        if (device != nullptr && done == lose_at)
            device->mark_lost(); // what a failed submission records on VK_ERROR_DEVICE_LOST
        return true;
    };
    return export_timeline_on(request, device, lose, stats) ? movie : QString();
}

// Frames composited before the loss on the GPU and after it on the CPU reference match an
// export rendered entirely on the CPU, frame for frame (independent ffmpeg decode). Where the
// export decoded in hardware (Intel), its pictures lived on the lost device: it must have
// switched to software decode at the loss.
bool export_continues_after_device_loss() {
    auto device = oma::gpu::Device::create();
    if (!device) {
        std::printf("    (skipped: no Vulkan device)\n");
        return true;
    }
    QTemporaryDir dir;
    ExportStats stats;
    const QString lost = export_camera(dir, "lost.mp4", device->get(), kLostAt, &stats);
    const QString cpu = export_camera(dir, "cpu.mp4", nullptr, -1);
    if (lost.isEmpty() || cpu.isEmpty() || !(*device)->lost() || decoded_frames(lost) != kFrames)
        return false;
    if (stats.decode_path.starts_with("hardware") &&
        !stats.decode_path.contains("software after device loss at frame 10"))
        return false;
    const QByteArray a = gray_frames(lost);
    const QByteArray b = gray_frames(cpu);
    if (a.size() != b.size() || a.size() != kFrame * kFrames)
        return false;
    for (int i = 0; i < kFrames; ++i) {
        // Each frame is the CPU export's frame `i`, within a lossy encode of compositors that
        // agree within 0.002 linear: under one gray level on average.
        long sad = 0;
        for (qsizetype p = 0; p < kFrame; ++p)
            sad += std::abs(static_cast<unsigned char>(a[(i * kFrame) + p]) -
                            static_cast<unsigned char>(b[(i * kFrame) + p]));
        if (closest(b, a, i) != i || sad > kFrame)
            return false;
    }
    return true;
}
} // namespace

void run_export_device_loss_tests() {
    describe("Export after GPU device loss", {
        it("finishes on the CPU reference with the same frames",
           { expect(export_continues_after_device_loss()).toBeTruthy(); });
    });
}
