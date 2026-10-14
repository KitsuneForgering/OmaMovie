// M10 concurrency and lifetime: seeded play/pause/seek/shuttle/step/edit/undo bursts while the
// viewer, audio and job worker run, an export cancelled mid-way, and a Session destroyed while
// playing and exporting. Bounded and deterministic; the TSan and ASan builds run it too.
#include "session.hpp"
#include "wait_for.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdint>

#include "oma_test.hpp"

namespace {
constexpr auto kCamera = "tests/fixtures/generated/h264_30fps_aac.mp4";
// Sanitizer builds run a frame or a Vulkan device creation many times slower: the bounds are
// about hangs, not speed (debug: cancel in tens of ms, shutdown in a few hundred).
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr int kSlow = 30;
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
constexpr int kSlow = 30;
#else
constexpr int kSlow = 1;
#endif
#else
constexpr int kSlow = 1;
#endif

void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

bool open_camera(Session& s) {
    s.setSilent(true);
    s.open(QFileInfo(kCamera).absoluteFilePath());
    return wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); });
}

// 200 seeded transport and edit intents, a few milliseconds apart: the session stays valid,
// responsive (each intent returns) and ends paused with its playhead inside the sequence.
bool survives_transport_bursts() {
    Session s;
    if (!open_camera(s))
        return false;
    std::uint32_t state = 2026;
    for (int i = 0; i < 200; ++i) {
        state = (state * 1664525U) + 1013904223U;
        switch ((state >> 24) % 8) {
        case 0:
            s.togglePlay();
            break;
        case 1:
            s.pause();
            break;
        case 2:
            s.seek(static_cast<double>((state >> 8) % 1000) / 1000.0 * s.duration());
            break;
        case 3:
            s.shuttle(((state >> 8) & 1U) != 0 ? 1 : -1);
            break;
        case 4:
            s.stepFrames(((state >> 8) & 1U) != 0 ? 3 : -3);
            break;
        case 5:
            s.splitAtPlayhead();
            break;
        case 6:
            s.undo();
            break;
        default:
            s.redo();
            break;
        }
        pump(static_cast<int>((state >> 4) % 8));
    }
    s.pause();
    pump(50);
    return !s.playing() && s.position() >= 0.0 && s.position() <= s.duration() + 1e-9 &&
           !s.clips().isEmpty();
}

// An export of 20 s cancelled after its first frames ends promptly and publishes nothing.
bool cancels_an_export_cleanly() {
    QTemporaryDir dir;
    Session s;
    if (!dir.isValid() || !open_camera(s))
        return false;
    s.selectMedia(0);
    for (int i = 0; i < 19; ++i)
        s.appendSelected(); // long enough that the cancel lands mid-export
    if (s.clips().size() != 20)
        return false;
    const QString movie = dir.filePath("cancelled.mp4");
    s.setExportEncoder("software");
    s.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    if (!wait_for([&] { return s.exportProgress() > 0.0; }, 30000 * kSlow))
        return false;
    s.cancelExport();
    const bool ended = wait_for([&] { return s.exportProgress() < 0; }, 10000 * kSlow);
    pump(50);
    return ended && !QFileInfo::exists(movie) && s.exportedFile() != movie &&
           QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty();
}

// Destroying a Session while it plays and exports stops its workers without hanging and leaves
// no temporary or stage behind (a movie finished before the end may stay).
bool shuts_down_while_busy() {
    QTemporaryDir dir;
    QElapsedTimer timer;
    {
        Session s;
        if (!dir.isValid() || !open_camera(s))
            return false;
        s.togglePlay();
        s.setExportEncoder("software");
        s.exportMovie(QUrl::fromLocalFile(dir.filePath("busy.mp4")), "mp4-h264");
        pump(30);
        timer.start();
    }
    const QStringList left =
        QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    return timer.elapsed() < 10000 * kSlow && (left.isEmpty() || left == QStringList{"busy.mp4"});
}
} // namespace

void run_session_stress_tests() {
    describe("Session under load", {
        it("survives seeded transport and edit bursts",
           { expect(survives_transport_bursts()).toBeTruthy(); });
        it("cancels an export cleanly", { expect(cancels_an_export_cleanly()).toBeTruthy(); });
        it("shuts down while playing and exporting",
           { expect(shuts_down_while_busy()).toBeTruthy(); });
    });
}
