// End to end (ADR-0006 export transfer): an ungraded clip of every limited-range level exports
// with the levels it came in with. Read as raw Y codes by ffmpeg, column by column; the old
// export curve turned level 125 into 108.
#include "luma_columns.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <cmath>

#include "oma_test.hpp"

namespace {

double worst_level_shift() {
    QTemporaryDir dir;
    const QString source =
        QFileInfo("tests/fixtures/generated/gray_ramp_bt709.mp4").absoluteFilePath();
    Session session;
    session.setSilent(true);
    session.open(source);
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return 999;
    const QString movie = dir.filePath("levels.mp4");
    session.setExportEncoder("software");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    if (!wait_for([&] { return session.exportProgress() < 0; }, 60000))
        return 999;
    const auto in = luma_columns(source);
    const auto out = luma_columns(movie);
    if (in.empty() || out.empty())
        return 999;
    double worst = 0;
    for (std::size_t x = 4; x + 4 < in.size(); ++x) // the edges blur against the frame border
        worst = std::max(worst, std::abs(out[x] - in[x]));
    return worst;
}

} // namespace

void run_export_levels_tests() {
    describe("Export levels", {
        it("exports an ungraded clip with the levels it came in with",
           { expect(worst_level_shift() <= 2.0).toBeTruthy(); });
    });
}
