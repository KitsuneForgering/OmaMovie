#include "frame_match.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

#include "oma_test.hpp"

namespace {
struct Exported {
    QString movie;
    ExportStats stats;
};

// `media` exported in software (deterministic libx264) with the Settings decode choice.
Exported export_with(const QTemporaryDir& dir, const char* media, const QString& decode) {
    Session session;
    session.setSilent(true);
    session.setExportDecode(decode);
    session.setExportEncoder("software");
    session.open(QFileInfo(QString("tests/fixtures/generated/") + media).absoluteFilePath());
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return {};
    const QString movie = dir.filePath(decode + ".mp4");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    if (!wait_for([&] { return session.exportProgress() < 0; }, 60000) ||
        session.exportedFile() != movie)
        return {};
    return {.movie = movie, .stats = session.lastExportStats()};
}

// Hardware decoding is bit-exact by specification: an export decoded in hardware (ADR-0022) must
// hash like the same export decoded in software (without VA-API both decode in software).
bool hardware_decode_matches_software(const char* media) {
    QTemporaryDir dir;
    const Exported hw = export_with(dir, media, "hardware");
    const Exported sw = export_with(dir, media, "software");
    if (hw.movie.isEmpty() || sw.movie.isEmpty() ||
        !sw.stats.decode_path.starts_with("software (chosen in Settings)"))
        return false;
    if (hw.stats.decoded.software_frames > 0)
        std::printf("    (%s: hardware decode unavailable, compared software with software)\n",
                    media);
    const QByteArray a = frame_hashes(hw.movie);
    return !a.isEmpty() && a.count('\n') > 30 && a == frame_hashes(sw.movie);
}
} // namespace

void run_export_decode_path_tests() {
    describe("Export decode paths", {
        it("exports 10-bit HEVC identically from hardware and software decode",
           { expect(hardware_decode_matches_software("hevc_10bit.mp4")).toBeTruthy(); });
        it("exports AV1 identically from hardware and software decode",
           { expect(hardware_decode_matches_software("av1_opus.mkv")).toBeTruthy(); });
    });
}
