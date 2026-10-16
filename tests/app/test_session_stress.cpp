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
#include <cstdio>

#include "oma_test.hpp"

namespace {
constexpr auto kCamera = "tests/fixtures/generated/h264_30fps_aac.mp4";
int stress_count(const char* name, int fallback, int maximum) {
    bool valid = false;
    const int configured = qEnvironmentVariableIntValue(name, &valid);
    return valid && configured > 0 && configured <= maximum ? configured : fallback;
}

std::uint32_t stress_seed(std::uint32_t fallback) {
    bool valid = false;
    const int configured = qEnvironmentVariableIntValue("OMA_STRESS_SEED", &valid);
    return valid && configured >= 0 ? static_cast<std::uint32_t>(configured) : fallback;
}
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

// Seeded transport and edit intents, a few milliseconds apart: the session stays valid,
// responsive (each intent returns) and ends paused with its playhead inside the sequence.
// Undo may legitimately remove the initial imported clip for some seeds.
bool survives_transport_bursts() {
    Session s;
    if (!open_camera(s))
        return false;
    const int operations = stress_count("OMA_STRESS_TRANSPORT_OPS", 200, 2000);
    std::uint32_t state = stress_seed(2026);
    std::printf("transport stress seed=%u operations=%d\n", state, operations);
    for (int i = 0; i < operations; ++i) {
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
    const bool valid = !s.playing() && s.position() >= 0.0 && s.position() <= s.duration() + 1e-9;
    if (!valid)
        std::printf("transport final playing=%d position=%.6f duration=%.6f clips=%lld\n",
                    s.playing(), s.position(), s.duration(),
                    static_cast<long long>(s.clips().size()));
    return valid;
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

// Seeded switches between a saved project, a new one and a fresh import, interleaved with
// playback and a started export, a few ms apart: results of replaced work are dropped
// (generation), and once everything settles the last request wins with its one clip.
bool survives_project_switches() {
    QTemporaryDir dir;
    const QString project = dir.filePath("saved.omamovie");
    {
        Session first;
        if (!dir.isValid() || !open_camera(first))
            return false;
        first.saveProject(QUrl::fromLocalFile(project));
        if (!wait_for([&] { return first.projectPath() == project && !first.dirty(); }))
            return false;
    }
    Session s;
    s.setSilent(true);
    s.setExportEncoder("software");
    const int operations = stress_count("OMA_STRESS_SWITCH_OPS", 40, 200);
    std::uint32_t state = stress_seed(7);
    std::printf("project-switch stress seed=%u operations=%d\n", state, operations);
    for (int i = 0; i < operations; ++i) {
        state = (state * 1664525U) + 1013904223U;
        switch ((state >> 24) % 5) {
        case 0:
            s.openProject(QUrl::fromLocalFile(project));
            break;
        case 1:
            s.newProject();
            break;
        case 2:
            s.open(QFileInfo(kCamera).absoluteFilePath());
            break;
        case 3:
            s.togglePlay();
            break;
        default:
            s.exportMovie(QUrl::fromLocalFile(dir.filePath(QStringLiteral("switch-%1.mp4").arg(i))),
                          "mp4-h264");
            break;
        }
        pump(static_cast<int>((state >> 4) % 12));
    }
    s.newProject();
    s.open(QFileInfo(kCamera).absoluteFilePath());
    const bool settled = wait_for(
        [&] { return s.clips().size() == 1 && !s.importsPending() && s.exportProgress() < 0; },
        60000 * kSlow);
    s.pause();
    return settled && s.media().size() == 1;
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
        it("survives seeded project switches during playback and export",
           { expect(survives_project_switches()).toBeTruthy(); });
        it("shuts down while playing and exporting",
           { expect(shuts_down_while_busy()).toBeTruthy(); });
    });
}
