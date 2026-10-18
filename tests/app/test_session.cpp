#include "../playback/playback_test.hpp"
#include "session.hpp"

#include "oma/media/probe.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include <cmath>
#include <functional>

#include "oma_test.hpp"

namespace {

namespace tl = oma::timeline;

// Runs the event loop until `done` holds or `ms` pass (imports finish on job workers and
// report back through queued calls).
bool wait_until(const std::function<bool()>& done, int ms = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

QString fixture(const char* name) {
    return QFileInfo(QStringLiteral("tests/fixtures/generated/") + QString::fromLatin1(name))
        .absoluteFilePath();
}

// A session with one 1 s 30 fps clip on the storyline (the fixture), or nothing on failure.
bool load_clip(Session& s) {
    s.setSilent(true);
    s.open(fixture("h264_30fps_aac.mp4"));
    return wait_until([&] { return s.clips().size() == 1 && !s.importsPending(); });
}

double clip_seconds(const Session& s, qsizetype index) {
    return s.clips().at(index).toMap().value("duration").toDouble();
}

// Source viewer (M6 pilot): a marked range is what Add uses; the frame under the playhead at
// Mark out is kept; undo removes the clip; marks persist for the session and are visible.
bool source_range_adds_exactly() {
    Session s;
    if (!load_clip(s))
        return false;
    s.openSource(0);
    s.seekSource(0.2); // frame 6
    s.markIn();
    s.stepFrames(9); // frame 15
    s.markOut();     // [6, 16)
    const double before = s.position();
    s.appendSelected();
    const bool exact = s.clips().size() == 2 && std::abs(clip_seconds(s, 1) - 10.0 / 30.0) < 1e-9 &&
                       s.position() >= before;
    s.undo();
    const bool undone = s.clips().size() == 1;
    const bool shown = std::abs(s.media().value(0).toMap().value("markIn").toDouble() - 0.2) < 1e-9;
    return exact && undone && shown;
}

bool precise_nudges_keep_one_edit_per_step() {
    Session s;
    if (!load_clip(s))
        return false;
    const double first = s.clips().front().toMap().value("id").toDouble();
    s.trimClip(first, false, -5); // leave five source frames as the left clip's handle
    s.openSource(0);
    s.seekSource(5.0 / 30.0);
    s.markIn();
    s.seekSource(24.0 / 30.0);
    s.markOut();
    s.appendSelected();
    if (s.clips().size() != 2)
        return false;
    const double second = s.clips().at(1).toMap().value("id").toDouble();
    const double cut = s.clips().at(1).toMap().value("start").toDouble();
    s.selectClip(first);
    s.nudgeRoll(-1);
    const bool rolled =
        std::abs(s.clips().at(1).toMap().value("start").toDouble() - (cut - 1.0 / 30.0)) < 1e-9;
    s.undo();
    const bool roll_undo = std::abs(s.clips().at(1).toMap().value("start").toDouble() - cut) < 1e-9;
    s.selectClip(second);
    const double source = s.clips().at(1).toMap().value("sourceIn").toDouble();
    s.nudgeSlip(1);
    const bool slipped = std::abs(s.clips().at(1).toMap().value("sourceIn").toDouble() -
                                  (source + 1.0 / 30.0)) < 1e-9 &&
                         std::abs(s.clips().at(1).toMap().value("start").toDouble() - cut) < 1e-9;
    s.undo();
    const bool slip_undo =
        std::abs(s.clips().at(1).toMap().value("sourceIn").toDouble() - source) < 1e-9;
    s.nudgeSlide(1);
    const bool slid =
        std::abs(s.clips().at(1).toMap().value("start").toDouble() - (cut + 1.0 / 30.0)) < 1e-9;
    s.undo();
    return rolled && roll_undo && slipped && slip_undo && slid &&
           std::abs(s.clips().at(1).toMap().value("start").toDouble() - cut) < 1e-9;
}

bool source_range_works_for_overwrite_and_connect() {
    Session s;
    if (!load_clip(s))
        return false;
    s.openSource(0);
    s.seekSource(0.2);
    s.markIn();
    s.seekSource(0.5);
    s.markOut(); // [6, 16): ten source frames
    s.seek(0);   // sequence playhead; source marks remain on the library item
    s.overwriteSelected();
    const QVariantList overwritten = s.clips();
    if (overwritten.isEmpty() ||
        std::abs(overwritten.front().toMap().value("duration").toDouble() - 10.0 / 30.0) > 1e-9 ||
        std::abs(overwritten.front().toMap().value("sourceIn").toDouble() - 0.2) > 1e-9)
        return false;
    s.seek(0.5);
    s.connectSelected();
    const QVariantList tracks = s.videoTracks();
    if (tracks.isEmpty())
        return false;
    const QVariantList connected = tracks.front().toMap().value("clips").toList();
    return connected.size() == 1 &&
           std::abs(connected.front().toMap().value("duration").toDouble() - 10.0 / 30.0) < 1e-9 &&
           std::abs(connected.front().toMap().value("sourceIn").toDouble() - 0.2) < 1e-9;
}

bool marked_source_range_survives_project_reopen() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString path = dir.filePath(QStringLiteral("marked.omamovie"));
    Session original;
    if (!load_clip(original))
        return false;
    original.openSource(0);
    original.seekSource(0.2);
    original.markIn();
    original.seekSource(0.5);
    original.markOut();
    original.appendSelected();
    original.saveProject(QUrl::fromLocalFile(path));
    if (!wait_until([&] { return original.projectPath() == path && !original.dirty(); }))
        return false;

    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(path));
    if (!wait_until([&] {
            return reopened.projectPath() == path && reopened.clips().size() == 2 &&
                   !reopened.importsPending();
        }))
        return false;
    const QVariantMap clip = reopened.clips().at(1).toMap();
    return std::abs(clip.value("sourceIn").toDouble() - 0.2) < 1e-9 &&
           std::abs(clip.value("duration").toDouble() - 10.0 / 30.0) < 1e-9;
}

bool mismatched_saved_media_stays_in_project(const char* source, const char* replacement) {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString media = dir.filePath(QStringLiteral("source.mp4"));
    const QString project = dir.filePath(QStringLiteral("edit.omamovie"));
    const QString saved_again = dir.filePath(QStringLiteral("again.omamovie"));
    if (!QFile::copy(fixture(source), media))
        return false;
    Session original;
    original.setSilent(true);
    original.open(media);
    if (!wait_until([&] { return original.media().size() == 1 && !original.importsPending(); }))
        return false;
    original.openSource(0);
    original.appendSelected();
    if (original.clips().size() < 2)
        return false;
    original.saveProject(QUrl::fromLocalFile(project));
    if (!wait_until([&] { return original.projectPath() == project && !original.dirty(); }))
        return false;
    if (!QFile::remove(media) || !QFile::copy(fixture(replacement), media))
        return false;

    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_until([&] { return reopened.media().size() == 1 && !reopened.importsPending(); }))
        return false;
    if (!reopened.media().front().toMap().value("missing").toBool() ||
        reopened.clips().size() < 2 ||
        !reopened.notice().contains(QStringLiteral("cannot stand in")))
        return false;
    reopened.saveProject(QUrl::fromLocalFile(saved_again));
    if (!wait_until([&] { return reopened.projectPath() == saved_again; }))
        return false;
    Session again;
    again.setSilent(true);
    again.openProject(QUrl::fromLocalFile(saved_again));
    return wait_until([&] { return again.media().size() == 1 && !again.importsPending(); }) &&
           again.media().front().toMap().value("missing").toBool() && again.clips().size() >= 2;
}

// Marks that leave nothing (out at or before in) never produce an empty clip.
bool reversed_marks_reset() {
    Session s;
    if (!load_clip(s))
        return false;
    s.openSource(0);
    s.seekSource(0.5);
    s.markOut();       // out after frame 15
    s.seekSource(0.6); // frame 18: past out
    s.markIn();        // in past out: out is dropped, the range runs to the end
    const QVariantMap src = s.source();
    return src.value("out").toDouble() < 0 && std::abs(src.value("in").toDouble() - 0.6) < 1e-9;
}

// A sequence seek (timeline, menus) leaves the source viewer; source seeks do not move the
// sequence playhead.
bool playheads_stay_apart() {
    Session s;
    if (!load_clip(s))
        return false;
    s.seek(0.1);
    const double sequence = s.position();
    s.openSource(0);
    s.seekSource(0.8);
    const bool apart =
        s.position() == sequence && std::abs(s.source().value("position").toDouble() - 0.8) < 1e-9;
    s.seek(0.3);
    return apart && !s.source().value("open").toBool();
}

// Ripple preview: simulated with the delete command itself, changes nothing.
bool ripple_preview_names_clips() {
    Session s;
    if (!load_clip(s))
        return false;
    s.selectMedia(0);
    s.appendSelected(); // a second clip after the first
    if (s.clips().size() != 2)
        return false;
    const QVariant first = s.clips().at(0).toMap().value("id");
    const QString second = s.clips().at(1).toMap().value("id").toString();
    s.selectClip(first.toDouble());
    const QString undo = s.undoText();
    s.previewDelete(true);
    const QVariantMap scope = s.editScope();
    const bool named = scope.value("removed").toList().contains(first) &&
                       scope.value("moved").toMap().value(second).toInt() == -1;
    const bool untouched = s.clips().size() == 2 && s.undoText() == undo;
    s.previewDelete(false); // lift: a gap stays, nothing moves
    const bool lift = s.editScope().value("moved").toMap().isEmpty();
    s.clearEditScope();
    return named && untouched && lift && !s.editScope().value("active").toBool();
}

// Captions (ADR-0017): added at the playhead up to the next one, never over another; imported
// from SRT onto the frame grid; exported again as SRT that reads the same.
bool captions_round_trip() {
    Session s;
    if (!load_clip(s))
        return false;
    QTemporaryDir dir;
    QFile srt(dir.filePath("in.srt"));
    if (!srt.open(QIODevice::WriteOnly))
        return false;
    srt.write(
        "1\n00:00:00,100 --> 00:00:00,450\nHello\n\n2\n00:00:00,500 --> 00:00:00,900\nWorld\n");
    srt.close();
    s.importCaptions(QUrl::fromLocalFile(srt.fileName()));
    const QVariantList c = s.captions();
    // 0.1 s → frame 3 (floor); 0.45 s → frame 14 (ceil), cut at the next start (frame 15).
    const bool imported =
        c.size() == 2 && std::abs(c[0].toMap().value("start").toDouble() - 0.1) < 1e-9 &&
        std::abs(c[0].toMap().value("duration").toDouble() - 11.0 / 30.0) < 1e-9 &&
        s.captionAt(0.2) == QStringLiteral("Hello") && s.captionAt(0.95).isEmpty();
    s.seek(0.2);
    const bool refused = s.addCaption() == 0; // a caption is already there
    s.seek(0.0);
    const double id = s.addCaption(); // [0, 0.1): up to the next caption
    const bool added =
        id > 0 && std::abs(s.captions().front().toMap().value("duration").toDouble() - 0.1) < 1e-9;
    s.setCaptionText(id, QStringLiteral("Olá"));
    s.exportCaptions(QUrl::fromLocalFile(dir.filePath("out.srt")));
    QFile out(dir.filePath("out.srt"));
    const bool written = out.open(QIODevice::ReadOnly) &&
                         out.readAll().startsWith("1\n00:00:00,000 --> 00:00:00,100\nOlá");
    s.undo(); // the text edit
    return imported && refused && added && written &&
           s.captionAt(0.05) == QStringLiteral("Caption");
}

// Canvas proportion (ADR-0010): 9:16 keeps the short side, follows undo, and survives a save.
bool canvas_aspect_changes() {
    Session s;
    if (!load_clip(s))
        return false; // 320x180 fixture
    const bool start = s.canvasWidth() == 320 && s.canvasHeight() == 180;
    s.setCanvasAspect(9, 16);
    const bool vertical = s.canvasWidth() == 180 && s.canvasHeight() == 320;
    s.undo();
    const bool back = s.canvasWidth() == 320 && s.canvasHeight() == 180;
    s.redo();
    return start && vertical && back && s.canvasWidth() == 180;
}

// Named speed ramps (M8): accelerate keeps the duration, burst takes two thirds; undo restores.
bool speed_ramps() {
    Session s;
    if (!load_clip(s))
        return false; // 30 frames
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.setSpeedRamp(0);
    const QVariantMap up = s.clips().front().toMap();
    const bool same = std::abs(up.value("duration").toDouble() - 1.0) < 1e-9 &&
                      up.value("timing").toString() == "Ramp";
    s.undo();
    s.setSpeedRamp(2);
    const bool burst = std::abs(clip_seconds(s, 0) - 20.0 / 30.0) < 1e-9;
    s.undo();
    return same && burst && std::abs(clip_seconds(s, 0) - 1.0) < 1e-9 &&
           s.clips().front().toMap().value("timing").toString().isEmpty();
}

// Opacity and volume keys (M8): a toggle keeps the value shown, a slider then sets the key at
// the playhead, a fade change adds no key, and removing the last key leaves its value.
bool scalar_keys() {
    Session s;
    if (!load_clip(s))
        return false;
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.seek(0.0);
    s.toggleOpacityKey();
    s.toggleVolumeKey();
    s.seek(0.5);
    s.setClipOpacity(0.2);
    s.setClipAudio(0.5, 0.0, 0.0, false);
    s.setClipAudio(0.5, 0.1, 0.0, false); // only the fade
    const QVariantMap here = s.motion();
    const bool keyed = here.value("opacityKeys").toInt() == 2 &&
                       here.value("gainKeys").toInt() == 2 &&
                       here.value("opacityKeyHere").toBool() &&
                       std::abs(here.value("gain").toDouble() - 0.5) < 1e-6;
    // The timeline marks each key where it falls in the clip.
    const QVariantList marks = s.clips().front().toMap().value("keys").toList();
    const bool marked = marks.size() == 4 && std::abs(marks.back().toDouble() - 0.5) < 1e-9;
    s.seek(0.2);
    const double between = s.motion().value("opacity").toDouble();
    const bool interpolated =
        between > 0.2 && between < 1.0 && !s.motion().value("opacityKeyHere").toBool();
    s.seek(0.5);
    s.toggleVolumeKey(); // removes the 0.5 s key
    s.seek(0.0);
    s.toggleVolumeKey(); // the last one: its 1.0 stays as the volume
    const QVariantMap after = s.motion();
    return keyed && marked && interpolated && after.value("gainKeys").toInt() == 0 &&
           std::abs(after.value("gain").toDouble() - 1.0) < 1e-6;
}

bool exports_other_formats() {
    Session s;
    if (!load_clip(s))
        return false;
    s.setExportEncoder(QStringLiteral("software"));
    QTemporaryDir dir;
    for (const auto& [id, extension, video, audio] :
         {std::tuple{"mkv-hevc", ".mkv", "hevc", "aac"},
          std::tuple{"webm-vp9", ".webm", "vp9", "vorbis"}}) {
        const QString file = dir.filePath(QString::fromLatin1(id) + QString::fromLatin1(extension));
        s.exportMovie(QUrl::fromLocalFile(file), QString::fromLatin1(id));
        if (!wait_until([&] { return s.exportProgress() < 0; }, 60000) || s.exportedFile() != file)
            return false;
        const auto info = oma::media::probe(file.toStdString());
        if (!info || !info->best_video || !info->best_audio ||
            info->streams[static_cast<std::size_t>(*info->best_video)].codec != video ||
            info->streams[static_cast<std::size_t>(*info->best_audio)].codec != audio)
            return false;
    }
    return true;
}

bool export_refuses_broken_audio() {
    namespace pt = playback_test;
    pt::Sequence seq = pt::make_sequence();
    if (!pt::place(seq, 0, 0, 1600).valid())
        return false;
    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("existing.mp4"));
    QFile previous(file);
    if (!previous.open(QIODevice::WriteOnly) || previous.write("previous") != 8)
        return false;
    previous.close();
    ExportRequest request{
        .timeline = seq.editor.timeline(),
        .paths = {{pt::kTone.value(), dir.filePath(QStringLiteral("missing.wav")).toStdString()}},
        .luts = {},
        .file = file.toStdString(),
        .width = 32,
        .height = 32,
        .frames = 1,
        .ticks_per_frame = 1600,
        .audio_rate = pt::rate(),
        .encoder = ExportRequest::Encoder::Software};
    const auto result = export_timeline(request, [](std::int64_t, std::int64_t) { return true; });
    if (result || !previous.open(QIODevice::ReadOnly))
        return false;
    return previous.readAll() == "previous" && QDir(dir.path()).entryList(QDir::Files).size() == 1;
}

bool export_refuses_audio_failure_after_good_block() {
    namespace pt = playback_test;
    pt::Sequence seq = pt::make_sequence();
    if (!pt::place(seq, 0, 0, 1600).valid())
        return false;
    const auto second = seq.editor.new_clip_id();
    const tl::edit::ClipSource source{.media = pt::kNoisy,
                                      .source_in = pt::s(0),
                                      .duration = pt::s(1600),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}};
    if (!seq.editor.execute(tl::edit::overwrite(seq.audio, second, pt::s(1600), source)))
        return false;
    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("existing.mp4"));
    QFile previous(file);
    if (!previous.open(QIODevice::WriteOnly) || previous.write("previous") != 8)
        return false;
    previous.close();
    ExportRequest request{
        .timeline = seq.editor.timeline(),
        .paths = {{pt::kTone.value(), pt::fixture("tone_44100.wav").string()},
                  {pt::kNoisy.value(), dir.filePath(QStringLiteral("missing.wav")).toStdString()}},
        .luts = {},
        .file = file.toStdString(),
        .width = 32,
        .height = 32,
        .frames = 2,
        .ticks_per_frame = 1600,
        .audio_rate = pt::rate(),
        .encoder = ExportRequest::Encoder::Software};
    ExportStats stats;
    const auto result =
        export_timeline(request, [](std::int64_t, std::int64_t) { return true; }, &stats);
    if (result || !previous.open(QIODevice::ReadOnly))
        return false;
    return stats.frames == 1 && previous.readAll() == "previous" &&
           QDir(dir.path()).entryList(QDir::Files).size() == 1;
}

bool export_allows_intentional_silence() {
    namespace pt = playback_test;
    QTemporaryDir dir;
    const auto render = [&](pt::Sequence& seq, const QString& name) {
        ExportRequest request{.timeline = seq.editor.timeline(),
                              .paths = {},
                              .luts = {},
                              .file = dir.filePath(name).toStdString(),
                              .width = 32,
                              .height = 32,
                              .frames = 1,
                              .ticks_per_frame = 1600,
                              .audio_rate = pt::rate(),
                              .encoder = ExportRequest::Encoder::Software};
        const auto result =
            export_timeline(request, [](std::int64_t, std::int64_t) { return true; });
        return result.has_value() && QFileInfo::exists(dir.filePath(name));
    };
    pt::Sequence empty = pt::make_sequence();
    if (!render(empty, QStringLiteral("empty.mp4")))
        return false;
    pt::Sequence muted = pt::make_sequence();
    tl::AudioProperties properties;
    properties.muted = true;
    if (!pt::place(muted, 0, 0, 1600, properties).valid() ||
        !render(muted, QStringLiteral("muted.mp4")))
        return false;
    pt::Sequence retimed = pt::make_sequence();
    const auto clip = pt::place(retimed, 0, 0, 3200);
    return clip.valid() &&
           // 7/11 has no whole decode rate, so it stays silent; 2x and 1/2 now play their sound.
           retimed.editor.execute(tl::edit::set_speed(clip, pt::q(7, 11), false)).has_value() &&
           render(retimed, QStringLiteral("retimed.mp4"));
}

bool export_captions_use_start_snapshot() {
    Session s;
    if (!load_clip(s))
        return false;
    s.seek(0);
    const double caption = s.addCaption();
    if (caption <= 0)
        return false;
    s.setCaptionText(caption, QStringLiteral("Before export"));
    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("movie.mp4"));
    s.setExportEncoder(QStringLiteral("software"));
    s.exportMovie(QUrl::fromLocalFile(file), QStringLiteral("mp4-h264"));
    s.setCaptionText(caption, QStringLiteral("After export started"));
    if (!wait_until([&] { return s.exportProgress() < 0; }, 60000) || s.exportedFile() != file)
        return false;
    QFile sidecar(dir.filePath(QStringLiteral("movie.srt")));
    return sidecar.open(QIODevice::ReadOnly) && sidecar.readAll().contains("Before export") &&
           s.captionAt(0).contains(QStringLiteral("After export started"));
}

bool export_without_captions_refuses_stale_sidecar() {
    Session s;
    if (!load_clip(s))
        return false;
    QTemporaryDir dir;
    const QString movie = dir.filePath(QStringLiteral("movie.mp4"));
    QFile existing(movie);
    QFile sidecar(dir.filePath(QStringLiteral("movie.srt")));
    if (!existing.open(QIODevice::WriteOnly) || existing.write("old movie") != 9 ||
        !sidecar.open(QIODevice::WriteOnly) || sidecar.write("old captions") != 12)
        return false;
    existing.close();
    sidecar.close();
    s.exportMovie(QUrl::fromLocalFile(movie), QStringLiteral("mp4-h264"));
    if (!existing.open(QIODevice::ReadOnly) || !sidecar.open(QIODevice::ReadOnly))
        return false;
    return s.exportProgress() < 0 && s.notice().contains(QStringLiteral("movie.srt")) &&
           existing.readAll() == "old movie" && sidecar.readAll() == "old captions";
}

} // namespace

void run_session_tests() {
    describe("Session (headless)", {
        it("adds exactly the marked source range",
           { expect(source_range_adds_exactly()).toBeTruthy(); });
        it("nudges roll, slip and slide with one undo step each",
           { expect(precise_nudges_keep_one_edit_per_step()).toBeTruthy(); });
        it("uses a marked source range for overwrite and connect",
           { expect(source_range_works_for_overwrite_and_connect()).toBeTruthy(); });
        it("preserves the placed source range after saving and reopening",
           { expect(marked_source_range_survives_project_reopen()).toBeTruthy(); });
        it("keeps a shorter replacement locatable across saves", {
            expect(
                mismatched_saved_media_stays_in_project("h264_long_gop.mp4", "h264_30fps_aac.mp4"))
                .toBeTruthy();
        });
        it("keeps a replacement of the wrong kind locatable across saves", {
            expect(mismatched_saved_media_stays_in_project("h264_long_gop.mp4", "tone_44100.wav"))
                .toBeTruthy();
        });
        it("keeps a replacement without the saved audio stream locatable across saves", {
            expect(mismatched_saved_media_stays_in_project("h264_30fps_aac.mp4",
                                                           "screenrecording_silent.mp4"))
                .toBeTruthy();
        });
        it("keeps an image replacement for a video locatable across saves", {
            expect(mismatched_saved_media_stays_in_project("h264_long_gop.mp4", "still.png"))
                .toBeTruthy();
        });
        it("drops an out mark that an in mark passes",
           { expect(reversed_marks_reset()).toBeTruthy(); });
        it("keeps the source and sequence playheads apart",
           { expect(playheads_stay_apart()).toBeTruthy(); });
        it("previews a delete with the delete command itself",
           { expect(ripple_preview_names_clips()).toBeTruthy(); });
        it("applies named speed ramps with undo", { expect(speed_ramps()).toBeTruthy(); });
        it("changes the canvas proportion with undo",
           { expect(canvas_aspect_changes()).toBeTruthy(); });
        it("exports other selected container and codec profiles",
           { expect(exports_other_formats()).toBeTruthy(); });
        it("leaves an existing movie untouched when audio decode fails",
           { expect(export_refuses_broken_audio()).toBeTruthy(); });
        it("discards a partial export after a later audio source fails",
           { expect(export_refuses_audio_failure_after_good_block()).toBeTruthy(); });
        it("exports an audio-free, muted or explicitly silent retimed timeline",
           { expect(export_allows_intentional_silence()).toBeTruthy(); });
        it("exports captions from the movie's start snapshot",
           { expect(export_captions_use_start_snapshot()).toBeTruthy(); });
        it("protects an existing sidecar when the new movie has no captions",
           { expect(export_without_captions_refuses_stale_sidecar()).toBeTruthy(); });
        it("adds, imports and exports captions on the frame grid",
           { expect(captions_round_trip()).toBeTruthy(); });
        it("keys opacity and volume at the playhead", { expect(scalar_keys()).toBeTruthy(); });
    });
}
