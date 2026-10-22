// Mixed frame rates as melt/Kdenlive write them (melt_mixed_rate.mlt): a 23.976 fps clip, then a
// 29.97 fps clip, in a 23.976 fps sequence; imported, saved, reopened, exported and matched to
// each source by an independent decode.
#include "frame_match.hpp"
#include "reopened_export.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <vector>

#include "oma_test.hpp"

namespace {
constexpr auto kSource24 = "tests/fixtures/generated/h264_23.976fps.mp4";
constexpr auto kSource30 = "tests/fixtures/generated/h264_29.97fps.mp4";

// Output frame 12 + k falls at source time k·1.25 frames of the 29.97 fps clip. OmaMovie shows
// the frame on screen then (ADR-0004: the last with pts <= t); melt shows the nearest, so it
// differs at 3.75, 8.75 and 13.75 (4, 9, 14) and at the half-frame ties.
bool keeps_each_clip_at_its_own_rate() {
    QTemporaryDir dir;
    const QString movie =
        export_reopened_import(dir, "tests/importers/fixtures/melt_mixed_rate.mlt", 2);
    const QByteArray a = gray_frames(QFileInfo(kSource24).absoluteFilePath());
    const QByteArray b = gray_frames(QFileInfo(kSource30).absoluteFilePath());
    const QByteArray out = movie.isEmpty() ? QByteArray() : gray_frames(movie);
    const std::vector<int> second{0, 1, 2, 3, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17};
    if (a.size() != 24 * kFrame || b.size() != 30 * kFrame || out.size() != 27 * kFrame)
        return false;
    for (int k = 0; k < 12; ++k)
        if (closest(a, out, k) != k)
            return false;
    for (int k = 0; k < 15; ++k)
        if (closest(b, out, 12 + k) != second[static_cast<std::size_t>(k)])
            return false;
    return true;
}
} // namespace

void run_shared_rates_tests() {
    describe("Shared cross-editor frame rates", {
        it("plays a 29.97 fps clip in a 23.976 fps melt project at its own rate",
           { expect(keeps_each_clip_at_its_own_rate()).toBeTruthy(); });
    });
}
