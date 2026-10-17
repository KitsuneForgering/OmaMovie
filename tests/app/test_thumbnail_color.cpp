#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include "oma_test.hpp"

namespace {
QString thumbnail(const Session& s) {
    return s.media().value(0).toMap().value("thumbnail").toString();
}

QImage picture(const QString& url) {
    return QImage(QUrl(url).toLocalFile()).convertToFormat(QImage::Format_RGBA8888);
}

// Full-range BT.709 patches: reading them as limited range redraws the library thumbnail with
// more contrast; the project reopens with that reading's thumbnail, and undo brings back the
// original picture.
bool thumbnail_follows_the_reading() {
    QTemporaryDir dir;
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/color_patches_bt709_pc.mkv").absoluteFilePath());
    if (!dir.isValid() || !wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    const QString first = thumbnail(s);
    const QImage tagged = picture(first);
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.setMediaColor(0, 1); // matrix as the file says, limited range
    if (tagged.isNull() || !wait_for([&] { return thumbnail(s) != first; }))
        return false;
    const QImage limited = picture(thumbnail(s));
    const QString project = dir.filePath("reading.omamovie");
    s.saveProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return !s.dirty(); }))
        return false;
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.clips().size() == 1 && !reopened.importsPending(); }))
        return false;
    const bool kept = picture(thumbnail(reopened)) == limited;
    const QString changed = thumbnail(s);
    s.undo();
    if (!wait_for([&] { return thumbnail(s) != changed; }))
        return false;
    return !limited.isNull() && limited != tagged && kept && picture(thumbnail(s)) == tagged;
}
} // namespace

void run_thumbnail_color_tests() {
    describe("Library thumbnails", {
        it("follow the media's color reading, through reopen and undo",
           { expect(thumbnail_follows_the_reading()).toBeTruthy(); });
    });
}
