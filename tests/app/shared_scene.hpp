#pragma once

// Checks for the shared cross-editor scenes (tools/bench/cross-editor at fixture scale): the
// expected source frame of each output frame, matched by an independent ffmpeg decode.
#include "frame_match.hpp"
#include "loudness_match.hpp"
#include "reopened_export.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// Source frames in output order, as start:length runs; a negative start marks `length` frames
// where the editors differ by design and are not checked.
inline std::vector<int> frames(std::initializer_list<std::pair<int, int>> runs) {
    std::vector<int> out;
    for (const auto& [start, length] : runs)
        for (int f = 0; f < length; ++f)
            out.push_back(start < 0 ? -1 : start + f);
    return out;
}

// tests/importers/fixtures/`name` with `clips` clips over `source` (`source_frames` long).
struct Scene {
    const char* name;
    qsizetype clips;
    const char* source;
    int source_frames;
    std::vector<int> expected;
    bool sound = false; // the 30 fps steps source: also check each frame's loudness step
    const char* encoder = "software";
};

// Imports, saves, reopens and exports the scene; each checked output frame must show (and play)
// its expected source frame. The sound window is 1200 samples, 11 whole cycles of the 440 Hz
// tone: the default 800 (7.33 cycles) misreads the source's own frame 22 as 21.78.
inline bool exports_scene(const Scene& s, std::string* route = nullptr) {
    QTemporaryDir dir;
    const QString movie = export_reopened_import(dir, QString("tests/importers/fixtures/") + s.name,
                                                 s.clips, s.encoder, route);
    const QByteArray picture = gray_frames(QFileInfo(s.source).absoluteFilePath());
    const QByteArray out = movie.isEmpty() ? QByteArray() : gray_frames(movie);
    if (picture.size() != s.source_frames * kFrame || out.size() != std::ssize(s.expected) * kFrame)
        return false;
    const std::vector<float> samples = s.sound ? first_channel(movie) : std::vector<float>();
    for (std::size_t i = 0; i < s.expected.size(); ++i) {
        const int k = static_cast<int>(i);
        if (s.expected[i] >= 0 &&
            (closest(picture, out, k) != s.expected[i] ||
             (s.sound && heard_frame(samples, k, 200, 1200) != s.expected[i])))
            return false;
    }
    return true;
}
