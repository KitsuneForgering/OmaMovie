#include "autosave.hpp"
#include "export_bundle.hpp"
#include "export_bundle_publish.hpp"
#include "export_recovery.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include "oma_test.hpp"

namespace {
bool put(const QString& path, const char* bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == QByteArray(bytes).size();
}

QByteArray get(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool crash_and_recover(PublishStep stop, bool keep) {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-kill");
    const QString movie = dir.filePath("movie.mp4");
    const QString sidecar = dir.filePath("movie.srt");
    if (!QDir().mkpath(stage) || !put(movie, "old movie") || !put(sidecar, "old captions") ||
        !put(stage + "/movie.mp4", "new movie") || !put(stage + "/movie.srt", "new captions"))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = movie.toStdString(),
                                         .sidecar = std::filesystem::path(sidecar.toStdString()),
                                         .had_movie = true,
                                         .had_sidecar = true};
    if (!export_recovery::remember(record))
        return false;
    const pid_t child = fork();
    if (child == 0) {
        const auto fault = [stop](PublishStep step) {
            if (step == stop)
                raise(SIGKILL);
            return std::error_code{};
        };
        (void)publish_export_bundle(record.stage, record.movie, record.stage / "movie.mp4",
                                    record.sidecar, record.stage / "movie.srt", fault);
        _exit(1);
    }
    if (child < 0)
        return false;
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
        return false;
    const auto pending = export_recovery::pending();
    if (pending.size() != 1 || pending.front().stage != record.stage ||
        (export_recovery::state(pending.front()) == export_recovery::State::Completed) != keep)
        return false;
    const auto resolved = keep ? export_recovery::keep_current(pending.front())
                               : export_recovery::restore_previous(pending.front());
    return resolved && export_recovery::pending().empty() && !QDir(stage).exists() &&
           get(movie) == (keep ? "new movie" : "old movie") &&
           get(sidecar) == (keep ? "new captions" : "old captions");
}

bool every_kill_point_recovers() {
    for (const auto step :
         {PublishStep::BackupMovie, PublishStep::BackupSidecar, PublishStep::PublishMovie,
          PublishStep::PublishSidecar, PublishStep::Completed}) {
        if (!crash_and_recover(step, step == PublishStep::Completed))
            return false;
    }
    return true;
}

bool session_offers_and_restores_interruption() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-ui");
    const QString movie = dir.filePath("movie.mp4");
    const QString sidecar = dir.filePath("movie.srt");
    if (!QDir().mkpath(stage) || !put(movie, "old movie") || !put(sidecar, "old captions") ||
        !put(stage + "/movie.mp4", "new movie") || !put(stage + "/movie.srt", "new captions"))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = movie.toStdString(),
                                         .sidecar = std::filesystem::path(sidecar.toStdString()),
                                         .had_movie = true,
                                         .had_sidecar = true};
    if (!export_recovery::remember(record))
        return false;
    const pid_t child = fork();
    if (child == 0) {
        const auto fault = [](PublishStep step) {
            if (step == PublishStep::PublishMovie)
                raise(SIGKILL);
            return std::error_code{};
        };
        (void)publish_export_bundle(record.stage, record.movie, record.stage / "movie.mp4",
                                    record.sidecar, record.stage / "movie.srt", fault);
        _exit(1);
    }
    if (child < 0)
        return false;
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFSIGNALED(status))
        return false;
    Session restarted;
    const auto offered = restarted.recoveredExports();
    if (offered.size() != 1 || offered.front().toMap().value("completed").toBool())
        return false;
    restarted.resolveInterruptedExport(offered.front().toMap().value("id").toString(), false);
    return wait_for([&] { return restarted.recoveredExports().empty(); }) &&
           get(movie) == "old movie" && get(sidecar) == "old captions";
}

bool active_export_is_not_offered() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-active");
    const QString movie = dir.filePath("movie.mp4");
    if (!QDir().mkpath(stage))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = movie.toStdString(),
                                         .sidecar = std::nullopt,
                                         .had_movie = false,
                                         .had_sidecar = false};
    if (!export_recovery::remember(record))
        return false;
    const int fd = ::open((record.stage / ".active").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    const bool locked = ::flock(fd, LOCK_EX | LOCK_NB) == 0;
    const bool hidden = locked && export_recovery::pending().empty();
    ::close(fd);
    const bool offered = export_recovery::pending().size() == 1;
    const bool removed = static_cast<bool>(export_recovery::restore_previous(record));
    return hidden && offered && removed;
}

bool pending_destination_is_blocked() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-pending");
    const QString movie = dir.filePath("movie.mp4");
    if (!QDir().mkpath(stage))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = movie.toStdString(),
                                         .sidecar = std::nullopt,
                                         .had_movie = false,
                                         .had_sidecar = false};
    if (!export_recovery::remember(record))
        return false;
    auto timeline = oma::timeline::Timeline::create(oma::frame_rates::k30,
                                                    oma::SampleRate::make(48000).value());
    if (!timeline)
        return false;
    ExportRequest request{.timeline = *timeline,
                          .paths = {},
                          .luts = {},
                          .file = movie.toStdString(),
                          .width = 32,
                          .height = 32,
                          .frames = 1,
                          .ticks_per_frame = 1600,
                          .audio_rate = oma::SampleRate::make(48000).value(),
                          .encoder = ExportRequest::Encoder::Software};
    const auto result = export_movie_bundle(
        std::move(request), std::nullopt, [](std::int64_t, std::int64_t) { return true; }, nullptr);
    const bool blocked = !result && export_recovery::pending().size() == 1 && !QFile::exists(movie);
    const bool removed = static_cast<bool>(export_recovery::restore_previous(record));
    return blocked && removed;
}

bool interrupted_recovery_can_resume() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-resume");
    const QString movie = dir.filePath("movie.mp4");
    const QString sidecar = dir.filePath("movie.srt");
    if (!QDir().mkpath(stage) || !put(stage + "/.publishing", "1") ||
        !put(stage + "/previous-movie", "old movie") ||
        !put(stage + "/previous-captions", "old captions") || !put(movie, "old movie") ||
        !put(sidecar, "new captions"))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = movie.toStdString(),
                                         .sidecar = std::filesystem::path(sidecar.toStdString()),
                                         .had_movie = true,
                                         .had_sidecar = true};
    if (!export_recovery::remember(record) || !export_recovery::restore_previous(record) ||
        get(movie) != "old movie" || get(sidecar) != "old captions")
        return false;
    if (!QDir().mkpath(stage) || !put(stage + "/.restored", "1") ||
        !export_recovery::remember(record))
        return false;
    return export_recovery::restore_previous(record) && export_recovery::pending().empty() &&
           !QDir(stage).exists();
}

bool orphan_record_is_cleared() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath(".omamovie-export-orphan");
    if (!QDir().mkpath(stage))
        return false;
    const export_recovery::Record record{.stage = stage.toStdString(),
                                         .movie = dir.filePath("movie.mp4").toStdString(),
                                         .sidecar = std::nullopt,
                                         .had_movie = false,
                                         .had_sidecar = false};
    if (!export_recovery::remember(record))
        return false;
    const auto journal =
        autosave::dir().parent_path() / "exports" / (record.stage.filename().string() + ".json");
    if (!QFile::exists(QString::fromStdString(journal.string())) ||
        !QDir(stage).removeRecursively())
        return false;
    return export_recovery::pending().empty() &&
           !QFile::exists(QString::fromStdString(journal.string()));
}
} // namespace

void run_export_crash_tests() {
    describe("Export process interruption", {
        it("recovers both outputs after SIGKILL at every publication step",
           { expect(every_kill_point_recovers()).toBeTruthy(); });
        it("offers interrupted outputs to a restarted session",
           { expect(session_offers_and_restores_interruption()).toBeTruthy(); });
        it("does not offer an active export as interrupted",
           { expect(active_export_is_not_offered()).toBeTruthy(); });
        it("blocks reuse of a destination awaiting recovery",
           { expect(pending_destination_is_blocked()).toBeTruthy(); });
        it("resumes recovery after a partial copy or cleanup",
           { expect(interrupted_recovery_can_resume()).toBeTruthy(); });
        it("clears a journal left after successful stage removal",
           { expect(orphan_record_is_cleared()).toBeTruthy(); });
    });
}
