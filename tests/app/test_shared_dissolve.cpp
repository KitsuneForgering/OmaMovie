// The frames inside melt's luma mix (melt_mix_23976.mlt), which test_shared_cuts.cpp skips: after
// the import centres the cut, output 10-13 mix source k (outgoing) with source k + 2 (incoming)
// at progress 0, 1/4, 1/2 and 3/4 (TransitionWindow::progress at each frame's start).
#include "blend_weight.hpp"
#include "reopened_export.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <cmath>
#include <utility>
#include <vector>

#include "oma_test.hpp"

namespace {
constexpr auto kSource = "tests/fixtures/generated/h264_23.976fps.mp4";

// On the reference host the weights read 0.05, 0.28, 0.50 and 0.73. Pairing the mix one source
// frame early or late is off by at least 0.17 somewhere, and sampling progress at frame centres
// by 0.125 more, so 0.1 separates them.
bool mixes_on_schedule() {
    QTemporaryDir dir;
    const QString movie =
        export_reopened_import(dir, "tests/importers/fixtures/melt_mix_23976.mlt", 2);
    const QByteArray source = gray_frames(QFileInfo(kSource).absoluteFilePath());
    const QByteArray out = movie.isEmpty() ? QByteArray() : gray_frames(movie);
    if (source.size() != 24 * kFrame || out.size() != 22 * kFrame)
        return false;
    std::vector<std::pair<int, int>> unmixed; // output frame, source frame
    for (int k = 0; k <= 21; ++k)
        if (k < 10 || k > 13)
            unmixed.emplace_back(k, k < 10 ? k : k + 2);
    const auto map = level_map({source}, {out}, unmixed);
    bool ok = true;
    for (int k = 10; k <= 13; ++k) {
        const double progress = (k - 10) / 4.0;
        ok = ok && std::abs(blend_weight({source}, {out}, map, k, k + 2, k) - progress) <= 0.1;
    }
    return ok;
}
} // namespace

void run_shared_dissolve_tests() {
    describe("Shared cross-editor dissolve", {
        it("mixes melt's luma transition on schedule after save and reopen",
           { expect(mixes_on_schedule()).toBeTruthy(); });
    });
}
