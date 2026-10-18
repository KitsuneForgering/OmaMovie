// M10 sustained workload (bounded): repeated open -> edit -> play -> save -> export -> close
// cycles in one process. File descriptors must come back to the same count (a leak there is
// deterministic); resident memory may wander with allocator caches, so only runaway growth fails.
// The per-cycle numbers are printed for the record.
#include "session.hpp"
#include "wait_for.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <vector>

#include "oma_test.hpp"

namespace {
// ASan keeps freed memory in quarantine, so resident size climbs by design there; LSan reports
// heap leaks in that build instead.
#if defined(__SANITIZE_ADDRESS__)
constexpr bool kRssMeaningful = false;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
constexpr bool kRssMeaningful = false;
#else
constexpr bool kRssMeaningful = true;
#endif
#else
constexpr bool kRssMeaningful = true;
#endif

// Every descriptor, sockets and pipes included (QDir hides their dangling links).
long open_fds() {
    std::error_code ec;
    return static_cast<long>(std::distance(std::filesystem::directory_iterator("/proc/self/fd", ec),
                                           std::filesystem::directory_iterator()));
}

long rss_kib() {
    QFile status("/proc/self/status");
    if (!status.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray& line : status.readAll().split('\n'))
        if (line.startsWith("VmRSS:"))
            return line.mid(6).trimmed().split(' ').value(0).toLong();
    return -1;
}

bool cycle(const QTemporaryDir& dir, int n) {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.seek(0.5);
    s.splitAtPlayhead();
    s.togglePlay();
    wait_for([] { return false; }, 150);
    s.pause();
    const QString project = dir.filePath(QStringLiteral("cycle-%1.omamovie").arg(n));
    s.saveProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return s.projectPath() == project && !s.dirty(); }))
        return false;
    const QString movie = dir.filePath(QStringLiteral("cycle-%1.mp4").arg(n));
    s.setExportEncoder("software");
    s.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    return wait_for([&] { return s.exportProgress() < 0; }, 60000) && s.exportedFile() == movie;
}

bool cycles_hold_their_resources() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const int kCycles = qEnvironmentVariableIntValue("OMA_SUSTAINED_CYCLES") > 0
                            ? qEnvironmentVariableIntValue("OMA_SUSTAINED_CYCLES")
                            : 16;
    constexpr int kFdWarm = 2;
    // The measured Mesa/FFmpeg pools can settle around cycle nine; begin the RSS bound there,
    // then watch seven more cycles for continuing growth. FDs should settle much sooner.
    constexpr int kRssWarm = 9;
    long fds = 0;
    std::vector<long> rss_after_warm;
    for (int n = 0; n < kCycles; ++n) {
        if (!cycle(dir, n))
            return false;
        wait_for([] { return false; }, 50); // queued callbacks of the destroyed Session
        const long f = open_fds();
        const long r = rss_kib();
        std::printf("    cycle %d: %ld fds, %ld KiB resident\n", n, f, r);
        if (n == kFdWarm - 1) {
            fds = f;
        }
        if (n >= kRssWarm - 1) rss_after_warm.push_back(r);
        if (n >= kFdWarm && f > fds) {
            return false;
        }
    }
    // The software Vulkan driver's allocator can release tens of MiB between adjacent cycles.
    // Compare windows instead of treating one RSS spike as sustained growth.
    if (kRssMeaningful && rss_after_warm.size() >= 6) {
        std::array<long, 3> first{rss_after_warm[0], rss_after_warm[1], rss_after_warm[2]};
        const std::size_t end = rss_after_warm.size();
        std::array<long, 3> last{rss_after_warm[end - 3], rss_after_warm[end - 2], rss_after_warm[end - 1]};
        std::sort(first.begin(), first.end());
        std::sort(last.begin(), last.end());
        if (last[1] > first[1] + (64 * 1024)) return false;
    }
    return true;
}
} // namespace

void run_sustained_tests() {
    describe("Sustained workload", {
        it("keeps file descriptors and memory bounded over repeated edit/export cycles",
           { expect(cycles_hold_their_resources()).toBeTruthy(); });
    });
}
