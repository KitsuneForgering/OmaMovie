#include "session.hpp"
#include "wait_for.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <csignal>
#include <sys/resource.h>

#include "file_bytes.hpp"

#include "oma_test.hpp"

namespace {
// Writes fail with EFBIG past `limit` bytes per file (RLIMIT_FSIZE) while the export runs: the
// previous movie and captions stay, no stage or recovery record is left, nothing is announced.
bool write_failure_keeps_previous_pair(rlim_t limit) {
    QTemporaryDir dir;
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.setCaptionText(session.addCaption(), QString(900, QLatin1Char('x')));
    const QString movie = dir.filePath("out.mp4");
    const QString captions = dir.filePath("out.srt");
    if (!put(movie, "old movie") || !put(captions, "old captions"))
        return false;
    rlimit saved{};
    getrlimit(RLIMIT_FSIZE, &saved);
    rlimit low = saved;
    low.rlim_cur = limit;
    setrlimit(RLIMIT_FSIZE, &low);
    session.setExportEncoder("software");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool finished = wait_for([&] { return session.exportProgress() < 0; }, 60000);
    setrlimit(RLIMIT_FSIZE, &saved);
    const auto stages =
        QDir(dir.path()).entryList({".omamovie-export-*"}, QDir::Dirs | QDir::Hidden);
    return finished && session.exportedFile() != movie && get(movie) == "old movie" &&
           get(captions) == "old captions" && stages.isEmpty() &&
           session.recoveredExports().isEmpty();
}
} // namespace

void run_export_write_fail_tests() {
    std::signal(SIGXFSZ, SIG_IGN); // a write past the limit fails instead of killing the test
    describe("Export write failures", {
        it("keeps the previous pair when the recovery record cannot be written",
           { expect(write_failure_keeps_previous_pair(1)).toBeTruthy(); });
        it("keeps the previous pair when the captions cannot be staged",
           { expect(write_failure_keeps_previous_pair(700)).toBeTruthy(); });
        it("keeps the previous pair when the movie cannot be written",
           { expect(write_failure_keeps_previous_pair(8192)).toBeTruthy(); });
    });
}
