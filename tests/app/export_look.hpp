#pragma once

// Helpers for test_export_looks.cpp: the camera clip exported with one look, read back by ffmpeg.
#include "../playback/playback_test.hpp"
#include "exporter.hpp"

#include "oma/gpu/device.hpp"
#include "oma/timeline/effects.hpp"

#include <QProcess>
#include <QTemporaryDir>

#include <string_view>

namespace export_look {
namespace pt = playback_test;

// The camera clip with `look` (none: empty), exported on `device` (nullptr: CPU reference).
inline QByteArray exported(const QTemporaryDir& dir, std::string_view look,
                           oma::gpu::Device* device) {
    pt::Sequence seq = pt::make_sequence();
    const auto id = pt::place_camera(seq, 0);
    auto video = seq.editor.timeline().find_clip(id)->video;
    if (const auto* d = oma::timeline::find_effect_definition(look))
        video.effects = {oma::timeline::make_effect(*d)};
    if (!seq.editor.execute(oma::timeline::edit::set_video(id, video)))
        return {};
    const QString movie = dir.filePath(QString::fromUtf8(look.data(), look.size()) + ".mp4");
    const ExportRequest request{.timeline = seq.editor.timeline(),
                                .paths = pt::paths(),
                                .luts = {},
                                .file = movie.toStdString(),
                                .width = 320,
                                .height = 180,
                                .frames = 6,
                                .ticks_per_frame = 1600,
                                .audio_rate = pt::rate(),
                                .encoder = ExportRequest::Encoder::Software};
    if (!export_timeline_on(request, device, [](std::int64_t, std::int64_t) { return true; }))
        return {};
    QProcess ffmpeg; // RGB thumbnails: looks are colour changes a grey picture would hide
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", movie, "-vf", "scale=80:45,format=rgb24", "-f",
                            "rawvideo", "-"});
    return ffmpeg.waitForFinished(20000) ? ffmpeg.readAllStandardOutput() : QByteArray();
}

inline double mean_diff(const QByteArray& a, const QByteArray& b) {
    if (a.isEmpty() || a.size() != b.size())
        return 999;
    long sum = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        sum += std::abs(static_cast<unsigned char>(a[i]) - static_cast<unsigned char>(b[i]));
    return static_cast<double>(sum) / static_cast<double>(a.size());
}

} // namespace export_look
