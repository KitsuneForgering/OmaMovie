#include "../playback/playback_test.hpp"
#include "edge_frames.hpp"
#include "session.hpp"

#include "oma/timeline/edit.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QUrl>

#include "oma_test.hpp"

namespace {
bool exists(const QVariant& url) {
    return QFileInfo::exists(QUrl(url.toString()).toLocalFile());
}

// Two clips cut from one file: a roll shows both sides of the cut; a slide of the first clip
// has nothing before it, so only its right side has a picture.
bool shows_both_sides_of_a_nudged_cut() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.seek(0.5);
    session.splitAtPlayhead();
    const double first = session.clips().front().toMap().value("id").toDouble();
    session.selectClip(first);
    session.nudgeRoll(-1);
    if (!wait_for([&] { return session.edgeFrames().value("serial").toInt() == 1; }))
        return false;
    const QVariantMap roll = session.edgeFrames();
    session.nudgeSlide(1);
    if (!wait_for([&] { return session.edgeFrames().value("serial").toInt() == 2; }))
        return false;
    const QVariantMap slide = session.edgeFrames();
    return exists(roll.value("left")) && exists(roll.value("right")) &&
           slide.value("left").toString().isEmpty() && exists(slide.value("right"));
}

// Both sides of a rolled cut carry the media's color reading, which image_at applies.
bool sides_carry_the_color_reading() {
    namespace pt = playback_test;
    pt::Sequence seq = pt::make_sequence();
    const auto first = pt::place_camera(seq, 0);
    if (!first.valid() || !seq.editor.execute(oma::timeline::edit::split(first, pt::s(24000))) ||
        !seq.editor.execute(oma::timeline::edit::set_media_color(
            pt::kCamera, oma::timeline::MediaColorMatrix::Bt709,
            oma::timeline::MediaColorRange::Full)))
        return false;
    const auto sides = edge_sides(seq.editor.timeline(), first, EdgeEdit::Roll, 1600, pt::paths());
    const auto read = [](const EdgeSide& e) {
        return !e.path.empty() && e.media.color_matrix == oma::timeline::MediaColorMatrix::Bt709 &&
               e.media.color_range == oma::timeline::MediaColorRange::Full;
    };
    return sides && read((*sides)[0]) && read((*sides)[1]);
}
} // namespace

void run_edge_frames_tests() {
    describe("Edge frames", {
        it("decodes the two frames around a rolled or slid cut",
           { expect(shows_both_sides_of_a_nudged_cut()).toBeTruthy(); });
        it("carries the media's color reading to both sides",
           { expect(sides_carry_the_color_reading()).toBeTruthy(); });
    });
}
