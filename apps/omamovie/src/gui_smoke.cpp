// The GUI smoke driver (gui_smoke.hpp), moved verbatim out of main(): the body below still names
// the startup objects as main() did, through the aliases at the top of run_gui_smoke.
//
// QtTest drives the keyboard checks. With Qt 6.11 its headers complete classes such as
// QBitArray after a standard-library SFINAE probe saw them incomplete, which GCC 16 reports
// (-Wsfinae-incomplete); the diagnostic is about Qt's header order, not this file.
#include "gui_smoke.hpp"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsfinae-incomplete"
#endif
#include <QTest>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "action_registry.hpp"
#include "app_settings.hpp"
#include "oma/media/format.hpp"
#include "autosave.hpp"
#include "platform/omarchy/theme.hpp"
#include "preview_item.hpp"
#include "session.hpp"
#include "single_instance.hpp"
#include "waveform_item.hpp"

#include "oma/gpu/device.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/project/document.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVulkanInstance>

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// --smoke: drives the editor through import, edits, undo/redo, playback and keyboard
// shortcuts, saving screenshots when OMA_GUI_SMOKE_*_SCREENSHOT name files. Each step waits for
// its condition (imports and frames are asynchronous) up to a deadline.
class SmokeRun : public QObject {
public:
    struct Step {
        const char* name;
        std::function<bool()> ready;
        std::function<void()> act;
    };

    SmokeRun(QGuiApplication& app, std::vector<Step> steps) : app_(app), steps_(std::move(steps)) {
        connect(&timer_, &QTimer::timeout, this, [this] { poll(); });
        timer_.start(50);
        since_.start();
    }

    // Milliseconds since the current step began, for steps that just need time to pass.
    [[nodiscard]] qint64 elapsed() const { return since_.elapsed(); }

private:
    void poll() {
        if (index_ >= steps_.size()) return;
        Step& step = steps_[index_];
        if (!step.ready()) {
            if (since_.elapsed() > 8000) {
                std::printf("GUI smoke: FAIL (timed out at '%s')\n", step.name);
                timer_.stop();
                app_.exit(1);
            }
            return;
        }
        step.act();
        ++index_;
        since_.restart();
    }

    QGuiApplication& app_;
    std::vector<Step> steps_;
    std::size_t index_ = 0;
    QTimer timer_;
    QElapsedTimer since_;
};


void write_text(const QString& path, const QByteArray& text) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(text);
}

void screenshot(QQuickWindow* window, const char* variable) {
    const QString path = qEnvironmentVariable(variable);
    if (!path.isEmpty()) window->grabWindow().save(path);
}

} // namespace


// A fake Omarchy for the smoke run: state like omarchy-theme-set leaves it, and the two scripts
// on PATH reading it.
void write_smoke_theme(const QString& root) {
    QDir(root).mkpath(QStringLiteral("state/theme"));
    QDir(root).mkpath(QStringLiteral("fontconfig"));
    QDir(root).mkpath(QStringLiteral("bin"));
    write_text(root + QStringLiteral("/state/theme/colors"), "background\t#101010\naccent\t#3ee8ff\n");
    write_text(root + QStringLiteral("/state/theme.name"), "smoke\n");
    write_text(root + QStringLiteral("/fontconfig/fonts.conf"), QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toUtf8());
    const QByteArray color = "#!/bin/sh\ncat '" + root.toUtf8() + "/state/theme/colors'\n";
    const QByteArray font = "#!/bin/sh\ncat '" + root.toUtf8() + "/fontconfig/fonts.conf'\n";
    write_text(root + QStringLiteral("/bin/omarchy-theme-color"), color);
    write_text(root + QStringLiteral("/bin/omarchy-font-current"), font);
    for (const char* script : {"/bin/omarchy-theme-color", "/bin/omarchy-font-current"}) {
        QFile::setPermissions(root + QLatin1String(script), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    qputenv("PATH", (root + QStringLiteral("/bin:")).toLocal8Bit() + qgetenv("PATH"));
}

int run_gui_smoke(const SmokeContext& context) {
    QGuiApplication& application = context.application;
    QQuickWindow* window = context.window;
    PreviewItem* preview = context.preview;
    Session& session = context.session;
    oma::gpu::Device* device = &context.device;
    ActionRegistry& actions = context.actions;
    AppSettings& app_settings = context.settings;
    QTemporaryDir& smoke_recordings = context.recordings;
    window->resize(1600, 900);
    session.setSilent(true);
    session.setNotifications(false);
    struct Results {
        bool imported = false, split = false, undone = false, redone = false, rippled = false;
        bool inserted = false, overwritten = false, trimmed = false, audio = false;
        bool volume = false, muted = false, gpu_viewer = false, shuttle = false;
        bool played = false, space = false, escape = false, stepped = false, keys_focused = true;
        bool controls = false, lanes = false, looks = false, transitions = false, dragdrop = false;
        bool tab = false, guard = false, grading = false, project = false, recordings = false, connected = false, timing = false;
        bool palette = false, palette_keys = false, theme = false, launch = false, settings = false, menu_keys = false, titles = false, autosave = false;
        bool device_lost = false, layout = true, key_workflow = false;
        qsizetype key_clips = 0;
        double key_in = 0;
        double after_steps = 0;
    } r;
    const auto clip_count = [&] { return session.clips().size(); };
    // Save and reopen (UX-04): what the session held before saving, compared after opening.
    QTemporaryDir project_dir;
    QTemporaryDir relink_dir;
    const QString project_file = project_dir.filePath(QStringLiteral("smoke vlog.omamovie"));
    struct Saved {
        std::uint64_t cache_before = 0;
        bool relinked = false;
        double sequence_at = 0;
        qsizetype clips_before_source = 0;
        double export_seconds = 0;
        bool export_started = false;
        bool saved_while_exporting = false;
        bool caption_added = false;
        int canvas_before = 0;
        qsizetype media = 0, clips = 0, lanes = 0;
        double duration = 0;
        QVariantMap first;
        bool snapping = false;
        QString font, next_font;
        QStringList launched, forwarded;
        double menu_clip = 0;
        bool menu_audio_muted = false;
        qsizetype overlay_clips = 0;
        qsizetype storyline_clips = 0;
        QString undo_before_title;
        int title_pixels = 0;
        qsizetype clips_after_edit = 0;
        QString crashed;
        double key_position = 0;
        bool text_ok = true;
        bool lost_playing = false;
        double lost_position = 0;
    } saved;
    const auto lane_count = [&] { return session.audioTracks().size(); };
    // The first clip of an audio lane, as QML sees it.
    const auto lane_clip = [&](qsizetype lane) {
        const QVariantList lanes = session.audioTracks();
        if (lane >= lanes.size()) return QVariantMap{};
        const QVariantList clips = lanes[lane].toMap().value("clips").toList();
        return clips.isEmpty() ? QVariantMap{} : clips.front().toMap();
    };
    // Keys count only while the window keeps the focus: Qt fires window shortcuts in the
    // active window, and the desktop may move the focus during the run. Synthetic modifier
    // chords are unreliable on Wayland (resolved against the real keyboard), so only plain
    // keys are pressed.
    // Whether the named menu is open with the keyboard focus inside it.
    const auto menu_has_focus = [&](const QString& name) {
        auto* menu = window->findChild<QObject*>(name);
        // The menu's popup item takes the keys (Up/Down/Enter/Escape) when no item has focus yet.
        auto* content = menu ? menu->property("contentItem").value<QQuickItem*>() : nullptr;
        const QQuickItem* popup = content ? content->parentItem() : nullptr;
        bool inside = false;
        for (const QQuickItem* i = window->activeFocusItem(); i != nullptr && popup != nullptr; i = i->parentItem()) {
            inside = inside || i == popup;
        }
        return menu != nullptr && menu->property("opened").toBool() && inside;
    };
    const auto press = [&](Qt::Key key) {
        r.keys_focused = r.keys_focused && QGuiApplication::focusWindow() == window;
        QTest::keyClick(window, key);
    };
    // A shortcut with modifiers, as a user types it.
    const auto chord = [&](Qt::Key key, Qt::KeyboardModifiers mods) {
        r.keys_focused = r.keys_focused && QGuiApplication::focusWindow() == window;
        QTest::keyClick(window, key, mods);
    };
    const auto click_button = [&](const char* name) {
        auto* button = window->findChild<QQuickItem*>(QString::fromLatin1(name));
        if (!button || !button->isVisible() || !button->isEnabled()) {
            std::printf("GUI smoke: unavailable button %s\n", name);
            return false;
        }
        const QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
        return true;
    };
    // Presses at `from`, moves to `to` in small steps as a hand would, and releases there.
    const auto drag_mouse = [&](QPointF from, QPointF to) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from.toPoint());
        for (int i = 1; i <= 12; ++i) QTest::mouseMove(window, (from + (to - from) * i / 12.0).toPoint(), 10);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to.toPoint(), 10);
    };
    // Visible items of a QML delegate kind, left to right. Delegates have no QObject parent, so
    // the visual tree is walked instead of findChildren.
    const auto items_named = [&](const char* name) {
        QList<QQuickItem*> items;
        std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
            if (item->objectName() == QLatin1String(name) && item->isVisible()) items.push_back(item);
            for (QQuickItem* child : item->childItems()) walk(child);
        };
        walk(window->contentItem());
        std::ranges::sort(items, {}, [](QQuickItem* i) { return i->mapToScene(QPointF()).x(); });
        return items;
    };
    const auto center_of = [](QQuickItem* i) { return i->mapToScene(QPointF(i->width() / 2, i->height() / 2)); };
    // At the current size and text scale: the timeline keeps 120 px and the transport's last
    // button ends inside the window.
    const auto large_text_fits = [&](const char* width) {
        screenshot(window, (std::string("OMA_GUI_SMOKE_TEXT_") + width + "_SCREENSHOT").c_str());
        auto* timeline = window->findChild<QQuickItem*>(QStringLiteral("timelinePanel"));
        const auto end = items_named("transportEnd");
        // Library labels stay inside their tile (a range label once ran into the next tile).
        bool labels = true;
        for (QQuickItem* label : items_named("mediaRange"))
            labels = labels && label->width() <= label->parentItem()->width() + 0.5 &&
                     label->property("contentWidth").toDouble() <= label->width() + 0.5;
        const bool ok = timeline && timeline->height() >= 120 && !end.isEmpty() && labels &&
                        end.front()->mapToScene(QPointF(end.front()->width(), 0)).x() <= window->width();
        if (!ok) std::printf("GUI smoke: 130 %% text does not fit at %s px (timeline %.0f px, transport end %.0f of %d)\n", width,
                             timeline ? timeline->height() : -1.0,
                             end.isEmpty() ? -1.0 : end.front()->mapToScene(QPointF(end.front()->width(), 0)).x(), window->width());
        return ok;
    };
    // Yellow samples in the lower part of the viewer (titles in the smoke run are yellow).
    const auto yellow_in_lower_viewer = [&] {
        const QImage grab = window->grabWindow();
        const QPointF origin = preview->mapToScene(QPointF(0, 0));
        const double k = grab.devicePixelRatio();
        int yellow = 0;
        for (int y = static_cast<int>(preview->height() * 0.55); y < static_cast<int>(preview->height() * 0.95); y += 2) {
            for (int x = 0; x < static_cast<int>(preview->width()); x += 2) {
                const QColor c = grab.pixelColor(static_cast<int>((origin.x() + x) * k), static_cast<int>((origin.y() + y) * k));
                if (c.red() > 200 && c.green() > 170 && c.blue() < 90) ++yellow;
            }
        }
        return yellow;
    };
    QTemporaryDir scratch; // the LUT file the grading step loads
    std::unique_ptr<SmokeRun> run;
    const auto after = [&](qint64 ms) { return [&run, ms] { return run->elapsed() >= ms; }; };
    run = std::make_unique<SmokeRun>(application, std::vector<SmokeRun::Step>{
        {"start", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_PROJECTS_SCREENSHOT");
             // Projects lists the recording; Edit puts it on a new project's storyline.
             const QVariantList recs = session.recordings();
             const auto edit = items_named("editRecording");
             r.recordings = recs.size() == 1 && !recs.front().toMap().value("growing").toBool() &&
                            recs.front().toMap().value("name").toString().startsWith("screenrecording-") &&
                            edit.size() == 1;
             if (r.recordings) {
                 QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center_of(edit.front()).toPoint());
             } else {
                 std::printf("GUI smoke: recordings not listed (%lld)\n", static_cast<long long>(recs.size()));
                 session.newProject();
                 session.open(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4"));
             }
         }},
        {"first import", [&] { return clip_count() == 1; }, [&] {
             session.open(QStringLiteral("tests/fixtures/generated/hevc_10bit.mp4"));
         }},
        {"second import", [&] { return clip_count() == 2; }, [&] {
             r.imported = std::abs(session.duration() - 2.0) < 1e-6;
             session.seek(0.5);
         }},
        {"split", after(400), [&] {
             screenshot(window, "OMA_GUI_SMOKE_WIDE_SCREENSHOT");
             // The viewer composited on the GPU and Qt drew it: the center of the viewer shows
             // the video (the fixture's test pattern), not the neutral background.
             const QImage grab = window->grabWindow();
             const QPointF center = preview->mapToScene(QPointF(preview->width() / 2, preview->height() / 2));
             const QColor pixel = grab.pixelColor(static_cast<int>(center.x() * grab.devicePixelRatio()),
                                                  static_cast<int>(center.y() * grab.devicePixelRatio()));
             r.gpu_viewer = preview->presentedFrames() > 0 && pixel != QColor(0x1b, 0x1b, 0x1d) &&
                            pixel != QColor(Qt::black);
             if (!r.gpu_viewer)
                 std::printf("GUI smoke: viewer presented %u frames, center %s\n", preview->presentedFrames(),
                             qPrintable(pixel.name()));
             session.splitAtPlayhead();
             r.split = clip_count() == 3 && session.undoText() == QStringLiteral("Split");
             session.undo();
             r.undone = clip_count() == 2 && session.canRedo();
             session.redo();
             r.redone = clip_count() == 3;
             // The left half stays selected: ripple delete closes the gap.
             session.deleteSelected(true);
             r.rippled = clip_count() == 2 && std::abs(session.duration() - 1.5) < 1e-6;
             // The library's first video (1 s) at the playhead, 0.5 s into the sequence.
             session.selectMedia(0);
             session.seek(0.5);
             session.insertSelected();
             r.inserted = clip_count() == 3 && std::abs(session.duration() - 2.5) < 1e-6;
             session.undo();
             session.overwriteSelected(); // covers the second clip exactly
             r.overwritten = clip_count() == 2 && std::abs(session.duration() - 1.5) < 1e-6;
             session.undo();
             const double first = session.clips().front().toMap().value("id").toDouble();
             session.trimClip(first, true, 5); // ripple: the sequence loses 5 frames
             r.trimmed = std::abs(session.duration() - (1.5 - 5.0 / 30.0)) < 1e-6;
             session.undo();
             r.trimmed = r.trimmed && std::abs(session.duration() - 1.5) < 1e-6;
             session.seek(0);
             (void)session.takeAudioPeak();
             session.togglePlay();
         }},
        {"play", after(700), [&] {
             r.played = session.position() > 0.2;
             // The 440 Hz fixture tone (amplitude ~0.125) must reach the mix, paced by audio.
             r.audio = session.audioRunning() && session.takeAudioPeak() > 0.05F;
             session.pause();
             session.seek(0);
         }},
        {"shuttle forward", after(300), [&] {
             // L twice: double speed. Half a second of wall time covers about a second.
             session.pause();
             session.seek(0);
             session.shuttle(1);
             session.shuttle(1);
         }},
        {"shuttle check", after(450), [&] {
             const double forward = session.position();
             r.shuttle = session.speed() == 2 && forward > 0.6;
             session.pause();
             session.seek(1.2);
             session.shuttle(-1); // J: reverse at normal speed
         }},
        {"reverse check", after(400), [&] {
             r.shuttle = r.shuttle && session.speed() == -1 && session.position() < 1.1;
             session.pause(); // K
             r.shuttle = r.shuttle && !session.playing();
             if (!r.shuttle) std::printf("GUI smoke: shuttle failed at %.3fs\n", session.position());
         }},
        {"volume", after(100), [&] {
             // The first clip carries the fixture's tone. Fades snap to whole frames.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.setClipAudio(0.5, 0.11, 0.0, false);
             const QVariantMap info = session.info();
             r.volume = session.undoText() == QStringLiteral("Audio Adjustments") &&
                        std::abs(info.value("gain").toDouble() - 0.5) < 1e-6 &&
                        std::abs(info.value("fadeIn").toDouble() - 3.0 / 30.0) < 1e-9;
             window->setProperty("drawer", QStringLiteral("volume"));
             session.setClipAudio(0.5, 0.11, 0.0, true); // mute: the mix must go silent
             session.seek(0);
             (void)session.takeAudioPeak();
             session.togglePlay();
         }},
        {"muted play", after(500), [&] {
             r.muted = session.audioRunning() && session.takeAudioPeak() == 0.0F;
             session.pause();
             screenshot(window, "OMA_GUI_SMOKE_VOLUME_SCREENSHOT");
             window->setProperty("drawer", QString());
             session.undo();
             session.undo();
             session.seek(0);
             window->requestActivate();
         }},
        {"sound import", after(100), [&] {
             session.open(QStringLiteral("tests/fixtures/generated/tone_44100.wav"));
         }},
        {"audio lanes", [&] { return lane_count() == 1; }, [&] {
             // Appended sound lands on the first lane; a connected sound at an occupied time gets a
             // new lane, and deleting the lane's only clip removes the lane again.
             bool ok = std::abs(lane_clip(0).value("start").toDouble()) < 1e-9 &&
                       std::abs(lane_clip(0).value("duration").toDouble() - 2.0) < 1e-6;
             session.seek(0);
             session.insertSelected();
             ok = ok && lane_count() == 2 && session.undoText() == QStringLiteral("Connect Audio");
             session.deleteSelected(true);
             ok = ok && lane_count() == 1 && clip_count() == 2;
             const double id = lane_clip(0).value("id").toDouble();
             session.moveClip(id, 0, 6);
             ok = ok && std::abs(lane_clip(0).value("start").toDouble() - 0.2) < 1e-9;
             session.trimClip(id, true, 3); // lanes are not magnetic: the start moves
             ok = ok && std::abs(lane_clip(0).value("start").toDouble() - 0.3) < 1e-9 &&
                  std::abs(session.duration() - 2.2) < 1e-6;
             session.undo();
             session.undo();
             ok = ok && std::abs(lane_clip(0).value("start").toDouble()) < 1e-9;
             session.moveClip(id, 1, 0); // to a new lane; the emptied one goes away
             ok = ok && lane_count() == 1 && session.undoText() == QStringLiteral("Move");
             session.undo();
             // Detaching the first storyline clip's sound: lane 1 is taken there, so a new lane.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.detachAudio();
             ok = ok && lane_count() == 2 && session.undoText() == QStringLiteral("Detach Audio") &&
                  !session.clips().front().toMap().value("hasAudio").toBool();
             const double sound = lane_clip(1).value("id").toDouble();
             const double before = lane_clip(1).value("duration").toDouble();
             session.trimClip(sound, false, -5); // the sound ends 5 frames before the picture
             ok = ok && std::abs(lane_clip(1).value("duration").toDouble() - (before - 5.0 / 30.0)) < 1e-6;
             session.undo();
             session.undo();
             ok = ok && lane_count() == 1 && session.clips().front().toMap().value("hasAudio").toBool();
             // Volume "More" on the sound: equalizer preset, noise reduction from the measured
             // floor (the waveform is ready by now), and normalize (the quiet tone hits the
             // maximum volume).
             session.selectClip(id);
             session.setClipEq(-6, 3, 2);
             QVariantMap info = session.info();
             ok = ok && info.value("eqLow").toDouble() == -6.0 && info.value("eqMid").toDouble() == 3.0;
             session.setClipNoise(0.5);
             info = session.info();
             const bool noise = info.value("noise").toDouble() == 0.5;
             session.normalizeClip();
             info = session.info();
             const bool normalized = info.value("gain").toDouble() == 4.0;
             if (!noise || !normalized)
                 std::printf("GUI smoke: noise %d normalize %d (%s)\n", noise, normalized, qPrintable(session.notice()));
             ok = ok && noise && normalized;
             window->setProperty("drawer", QStringLiteral("volume"));
             window->setProperty("volumeMore", true);
             r.lanes = ok;
         }},
        {"volume more", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_LANES_SCREENSHOT");
             window->setProperty("drawer", QString());
             window->setProperty("volumeMore", false);
             session.undo();
             session.undo();
             session.undo();
             window->requestActivate();
         }},
        {"video adjustments", after(100), [&] {
             // Color, filter and framing on the first storyline clip, each one history entry.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.seek(0.1);
             session.setClipColor(0.5, 0.2, -0.3, 0.4);
             session.setClipEffectParam(QStringLiteral("oma.look.sepia"), QStringLiteral("amount"), 0.8);
             session.setClipFraming(1, 0.1, 0, 0.1, 0);
             session.setClipSharpness(-0.3);
             // The effect stack (ADR-0016): a second look, moved before the first, then bypassed.
             session.setClipEffect(QStringLiteral("oma.look.cool"), true);
             session.moveClipEffect(QStringLiteral("oma.look.cool"), -1);
             session.setClipEffectEnabled(QStringLiteral("oma.look.cool"), false);
             const QVariantMap info = session.info();
             const QVariantList stack = info.value("effects").toList();
             r.looks = info.value("exposure").toDouble() == 0.5 && stack.size() == 2 &&
                       stack.value(0).toMap().value("id").toString() == QStringLiteral("oma.look.cool") &&
                       !stack.value(0).toMap().value("enabled").toBool() &&
                       stack.value(1).toMap().value("amount").toDouble() == 0.8 &&
                       info.value("fit").toInt() == 1 && info.value("cropLeft").toDouble() == 0.1 &&
                       info.value("sharpness").toDouble() == -0.3 &&
                       session.undoText() == QStringLiteral("Video Adjustments");
             window->setProperty("drawer", QStringLiteral("effects"));
             session.requestFilterPreviews();
         }},
        {"filter previews", [&] { return session.filterPreviews().size() == Session::lookEffects().size(); }, [&] {
             r.looks = r.looks && std::ranges::all_of(session.filterPreviews(), [](const QVariant& v) {
                 return !v.toString().isEmpty();
             });
             window->resize(640, 620); // the narrowest width the UI promises (UX-06)
         }},
        {"effects drawer", after(400), [&] {
             screenshot(window, "OMA_GUI_SMOKE_EFFECTS_SCREENSHOT");
             auto* narrow_timeline = window->findChild<QQuickItem*>(QStringLiteral("timelinePanel"));
             r.layout = r.layout && narrow_timeline && narrow_timeline->height() >= 120;
             if (!r.layout) std::printf("GUI smoke: timeline too small at 640 px\n");
             // At 640 px two whole look tiles stay visible beside the stack and the slider.
             const auto narrow_tiles = items_named("lookTiles");
             const auto sharpen = items_named("sharpnessControl");
             const bool fits = !narrow_tiles.isEmpty() && narrow_tiles.front()->width() >= 2 * 92 + 8 && !sharpen.isEmpty() &&
                               sharpen.front()->mapToScene(QPointF(sharpen.front()->width(), 0)).x() <= window->width();
             if (!fits) std::printf("GUI smoke: effects drawer does not fit at 640 px\n");
             r.looks = r.looks && fits;
             window->resize(820, 620);
             // UX-05: a look tile takes keyboard focus and Enter toggles its look.
             const auto tiles = items_named("look:oma.look.black-and-white");
             if (!tiles.isEmpty()) {
                 tiles.front()->forceActiveFocus(Qt::TabFocusReason);
                 press(Qt::Key_Return);
             }
             const QVariantList stack = session.info().value("effects").toList();
             const bool keyed = std::ranges::any_of(stack, [](const QVariant& e) {
                 return e.toMap().value("id").toString() == QStringLiteral("oma.look.black-and-white");
             });
             if (!keyed) std::printf("GUI smoke: Enter on a look tile did not add the look (%lld tiles)\n",
                                     static_cast<long long>(tiles.size()));
             r.looks = r.looks && keyed;
             window->setProperty("drawer", QString());
             for (int i = 0; i < 8; ++i) session.undo();
             r.looks = r.looks && !session.info().value("colorAdjusted").toBool() &&
                       !session.info().value("filtered").toBool();
             window->requestActivate();
         }},
        {"keyframes", after(100), [&] {
             // Ken Burns keys the first clip from its start to 120% at its end; a key added at the
             // playhead takes the sliders' transform; undo removes all of it.
             const QVariantMap first = session.clips().front().toMap();
             const double end = first.value("start").toDouble() + first.value("duration").toDouble();
             session.selectClip(first.value("id").toDouble());
             session.kenBurns();
             session.seek(0);
             bool ok = session.motion().value("keys").toInt() == 2 && session.motion().value("keyHere").toBool() &&
                       std::abs(session.motion().value("scale").toDouble() - 1.0) < 1e-9;
             session.seek(end);
             ok = ok && std::abs(session.motion().value("scale").toDouble() - 1.2) < 1e-9;
             session.seek(end / 2);
             const double middle = session.motion().value("scale").toDouble();
             ok = ok && middle > 1.0 && middle < 1.2 && !session.motion().value("keyHere").toBool();
             session.toggleTransformKey();
             session.setClipTransform(50, 0, 1.5, 0);
             ok = ok && session.motion().value("keys").toInt() == 3 && session.motion().value("keyHere").toBool() &&
                  session.motion().value("posX").toDouble() == 50.0 && session.info().value("framingAdjusted").toBool();
             r.looks = r.looks && ok;
             if (!ok) std::printf("GUI smoke: keyframes failed\n");
             window->setProperty("drawer", QStringLiteral("crop"));
             window->setProperty("cropMore", true);
         }},
        {"keyframes drawer", after(400), [&] {
             screenshot(window, "OMA_GUI_SMOKE_KEYFRAMES_SCREENSHOT");
             window->setProperty("cropMore", false);
             window->setProperty("drawer", QStringLiteral("info"));
         }},
        {"dopesheet", after(300), [&] {
             // The Info drawer's dopesheet shows the three transform keys; clicking the last
             // one moves the playhead onto it (the clip's end, where Ken Burns put it).
             screenshot(window, "OMA_GUI_SMOKE_DOPESHEET_SCREENSHOT");
             auto keys = items_named("dopeKey");
             keys.removeIf([](const QQuickItem* i) { return !i->isVisible(); });
             const QVariantMap first = session.clips().front().toMap();
             const double end = first.value("start").toDouble() + first.value("duration").toDouble();
             if (keys.size() == 3) {
                 (void)window->grabWindow(); // click what was drawn (see the Picture menu check)
                 const auto last = std::ranges::max(keys, {}, [&](QQuickItem* k) { return center_of(k).x(); });
                 QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center_of(last).toPoint());
             }
             const bool ok = keys.size() == 3 && std::abs(session.position() - end) < 1.0 / session.frameRate();
             if (!ok) std::printf("GUI smoke: dopesheet failed (%lld keys, playhead %.3f, end %.3f)\n",
                                  static_cast<long long>(keys.size()), session.position(), end);
             r.looks = r.looks && ok;
             window->setProperty("drawer", QString());
             for (int i = 0; i < 3; ++i) session.undo();
             session.seek(0.3);
             actions.trigger(QStringLiteral("marker"));
             session.seek(0);
         }},
        {"marker flag", after(250), [&] {
             // M left a flag at 0.3 s on the timeline; clicking it brings the playhead back.
             auto flags = items_named("markerFlag");
             flags.removeIf([](const QQuickItem* i) { return !i->isVisible(); });
             if (flags.size() == 1) {
                 (void)window->grabWindow(); // click what was drawn
                 QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center_of(flags.front()).toPoint());
             }
             const bool ok = flags.size() == 1 && std::abs(session.position() - 0.3) < 1.0 / session.frameRate();
             if (!ok) std::printf("GUI smoke: marker flag failed (%lld flags, playhead %.3f)\n",
                                  static_cast<long long>(flags.size()), session.position());
             r.looks = r.looks && ok;
             session.undo();
             r.looks = r.looks && session.motion().value("keys").toInt() == 0;
             window->requestActivate();
         }},
        {"grading", after(100), [&] {
             // Wheels round-trip through the CDL, curve points edit one command each, and a .cube
             // file (a channel rotation) loads in the background onto the selected clip.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.seek(0.1);
             const QVariantMap wanted{{"lift", QVariantMap{{"x", 0.3}, {"y", -0.2}, {"level", 0.1}}},
                                      {"gamma", QVariantMap{{"x", 0.0}, {"y", 0.5}, {"level", -0.2}}},
                                      {"gain", QVariantMap{{"x", -0.4}, {"y", 0.1}, {"level", 0.3}}}};
             session.setClipWheels(wanted);
             const QVariantMap got = session.info().value("wheels").toMap();
             bool ok = session.info().value("graded").toBool();
             for (const QString& name : {QStringLiteral("lift"), QStringLiteral("gamma"), QStringLiteral("gain")}) {
                 for (const char* key : {"x", "y", "level"}) {
                     ok = ok && std::abs(got.value(name).toMap().value(key).toDouble() -
                                         wanted.value(name).toMap().value(key).toDouble()) < 2e-3;
                 }
             }
             const auto master = [&] { return session.info().value("curves").toMap().value("0").toList(); };
             session.addCurvePoint(0, 0.5, 0.6);
             ok = ok && master().size() == 3 && session.curveSamples(0, 3).at(1).toDouble() > 0.59;
             session.moveCurvePoint(0, 1, 0.5, 0.4);
             ok = ok && std::abs(master().at(1).toMap().value("y").toDouble() - 0.4) < 1e-9;
             session.removeCurvePoint(0, 1);
             ok = ok && master().isEmpty(); // a straight diagonal is no curve
             session.addCurvePoint(2, 0.3, 0.45);
             QFile cube(scratch.filePath(QStringLiteral("rotate.cube")));
             if (cube.open(QIODevice::WriteOnly)) {
                 cube.write("TITLE \"rotate\"\nLUT_3D_SIZE 2\n");
                 for (int b = 0; b < 2; ++b)
                     for (int g = 0; g < 2; ++g)
                         for (int red = 0; red < 2; ++red) cube.write(QStringLiteral("%1 %2 %3\n").arg(g).arg(b).arg(red).toUtf8());
                 cube.close();
             }
             session.importLut(QUrl::fromLocalFile(cube.fileName()));
             window->setProperty("drawer", QStringLiteral("color"));
             window->setProperty("colorMore", true);
             window->setProperty("gradeTab", QStringLiteral("curves"));
             window->setProperty("curveChannel", 2);
             r.grading = ok;
             if (!ok) std::printf("GUI smoke: wheels or curves failed\n");
         }},
        {"lut import", [&] { return session.luts().size() == 1; }, [&] {
             const QVariantMap info = session.info();
             r.grading = r.grading && info.value("lut").toDouble() > 0 && info.value("lutAmount").toDouble() == 1.0;
         }},
        {"grading drawer", after(400), [&] {
             screenshot(window, "OMA_GUI_SMOKE_GRADING_SCREENSHOT");
             window->setProperty("gradeTab", QStringLiteral("wheels"));
         }},
        {"grading wheels", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_WHEELS_SCREENSHOT");
             for (int i = 0; i < 6; ++i) session.undo(); // wheels, three curve edits, a point, the LUT
             r.grading = r.grading && !session.info().value("graded").toBool();
             window->setProperty("drawer", QString());
             window->setProperty("colorMore", false);
             window->requestActivate();
         }},
        {"transition", after(100), [&] {
             // The storyline clips touch at 0.5 s with no media to spare there, so a dissolve
             // stays a plain cut. Trimming 6 frames off each side of the cut gives both clips
             // handles: the dissolve then spans 12 frames, shortened from its 1 s.
             session.seek(0.45);
             session.addDissolveAtPlayhead();
             r.transitions = session.clips().at(1).toMap().value("transitionKind").toInt() == -1;
             session.undo();
             const double first = session.clips().at(0).toMap().value("id").toDouble();
             const double second = session.clips().at(1).toMap().value("id").toDouble();
             session.trimClip(first, false, -6);
             session.trimClip(second, true, 6);
             session.addDissolveAtPlayhead();
             const QVariantMap b = session.clips().at(1).toMap();
             r.transitions = r.transitions && session.undoText() == QStringLiteral("Transition") &&
                             b.value("transitionKind").toInt() == 0 &&
                             std::abs(b.value("transitionSpan").toDouble() - 12.0 / 30.0) < 1e-6;
             session.setTransition(second, 2, 1.0);
             r.transitions = r.transitions && session.clips().at(1).toMap().value("transitionKind").toInt() == 2;
             session.seek(0.2);
             session.togglePlay();
         }},
        {"transition playback", after(500), [&] {
             session.pause();
             screenshot(window, "OMA_GUI_SMOKE_TRANSITION_SCREENSHOT");
             for (int i = 0; i < 4; ++i) session.undo();
             r.transitions = r.transitions && !session.clips().at(1).toMap().value("transitionSet").toBool() &&
                             std::abs(session.duration() - 2.0) < 1e-6 && !session.failed();
             if (!r.transitions) std::printf("GUI smoke: transitions failed (%s)\n", qPrintable(session.status()));
             window->requestActivate();
         }},
        {"drag and drop", after(100), [&] {
             // Dragging the first storyline clip past the last cut reorders the two; library items
             // dropped on the timeline land on the nearest cut (pictures) or the lane under the
             // pointer, or a new lane when that one is taken (sound).
             const double a = session.clips().at(0).toMap().value("id").toDouble();
             const double b = session.clips().at(1).toMap().value("id").toDouble();
             const double length = session.duration();
             session.reorderClip(a, length);
             bool ok = session.clips().at(0).toMap().value("id").toDouble() == b &&
                       session.clips().at(1).toMap().value("id").toDouble() == a &&
                       session.undoText() == QStringLiteral("Move") && std::abs(session.duration() - length) < 1e-9;
             session.undo();
             ok = ok && session.clips().at(0).toMap().value("id").toDouble() == a && session.storylineCut(0.1, 0) == 0.0;
             const QVariantList media = session.media();
             const auto index_of = [&](bool audio) {
                 for (qsizetype i = 0; i < media.size(); ++i)
                     if (media[i].toMap().value("audioOnly").toBool() == audio) return static_cast<int>(i);
                 return -1;
             };
             const auto clips = clip_count();
             session.dropMedia(index_of(false), 0.05, -1);
             ok = ok && clip_count() == clips + 1 && session.undoText() == QStringLiteral("Insert") &&
                  session.clips().at(0).toMap().value("id").toDouble() == session.selectedClip();
             session.undo();
             const auto lanes = lane_count();
             session.dropMedia(index_of(true), 0.5, 0);
             ok = ok && lane_count() == lanes + 1 && session.undoText() == QStringLiteral("Connect Audio");
             session.undo();
             // Up and Down step through the storyline clips, moving the playhead to their start.
             session.selectClip(a);
             session.selectAdjacentClip(1);
             ok = ok && session.selectedClip() == b &&
                  std::abs(session.position() - session.clips().at(1).toMap().value("start").toDouble()) < 1e-9;
             session.selectAdjacentClip(-1);
             ok = ok && session.selectedClip() == a && session.position() == 0.0;
             // Snapping: either end of a dragged span within reach of the cut lands on it.
             const double cut = session.clips().at(1).toMap().value("start").toDouble();
             const QVariantMap head = session.snapSpan(cut + 0.02, 0.3, 0, 0.05);
             const QVariantMap tail = session.snapSpan(cut - 0.29, 0.3, 0, 0.05);
             const QVariantMap free = session.snapSpan(cut + 0.2, 0, 0, 0.05);
             ok = ok && head.value("start").toDouble() == cut && head.value("line").toDouble() == cut &&
                  std::abs(tail.value("start").toDouble() - (cut - 0.3)) < 1e-9 &&
                  free.value("line").toDouble() == -1 && free.value("start").toDouble() == cut + 0.2;
             r.dragdrop = ok && clip_count() == clips && lane_count() == lanes;
             if (!r.dragdrop) std::printf("GUI smoke: drag and drop failed (%s)\n", qPrintable(session.status()));
             window->setProperty("libraryOverlay", true); // the tiling desktop may make the window narrow
         }},
        {"mouse gestures", after(100), [&] {
             // The same gestures with the mouse: a clip dragged past the last one, a library item
             // dragged onto the start of the storyline, and a right click opening the clip menu.
             auto clips = items_named("storylineClip");
             const auto library = items_named("libraryItem");
             bool ok = clips.size() == 2 && !library.empty();
             if (ok) {
                 const double a = session.clips().at(0).toMap().value("id").toDouble();
                 const QPointF last_end = clips[1]->mapToScene(QPointF(clips[1]->width() - 2, clips[1]->height() / 2));
                 // Before the drop, the clip that will move shows it (and which way).
                 const QPointF from = center_of(clips[0]);
                 QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from.toPoint());
                 for (int i = 1; i <= 12; ++i) QTest::mouseMove(window, (from + (last_end - from) * i / 12.0).toPoint(), 10);
                 const bool previewed = items_named("willMove").size() == 1;
                 QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, last_end.toPoint(), 10);
                 if (!previewed) std::printf("GUI smoke: no move preview while dragging\n");
                 ok = previewed && session.clips().at(1).toMap().value("id").toDouble() == a &&
                      session.undoText() == QStringLiteral("Move");
                 session.undo();
                 clips = items_named("storylineClip");
                 const auto count = clip_count();
                 drag_mouse(center_of(library.front()), clips[0]->mapToScene(QPointF(4, clips[0]->height() / 2)));
                 ok = ok && clip_count() == count + 1 && session.undoText() == QStringLiteral("Insert");
                 session.undo();
                 clips = items_named("storylineClip");
                 QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, center_of(clips[0]).toPoint());
                 auto* menu = window->findChild<QObject*>(QStringLiteral("storylineMenu"));
                 ok = ok && menu != nullptr && menu->property("opened").toBool();
                 if (menu != nullptr) QMetaObject::invokeMethod(menu, "close");
             }
             window->setProperty("libraryOverlay", false);
             r.dragdrop = r.dragdrop && ok;
             if (!ok) std::printf("GUI smoke: mouse gestures failed\n");
         }},
        {"space", after(300), [&] { press(Qt::Key_Space); }},
        {"space again", after(250), [&] {
             r.space = session.playing();
             press(Qt::Key_Space);
         }},
        {"escape", after(250), [&] {
             r.space = r.space && !session.playing();
             window->setProperty("viewerOnly", true);
             press(Qt::Key_Escape);
         }},
        {"frames", after(250), [&] {
             r.escape = !window->property("viewerOnly").toBool();
             // Without focus the key never arrived: leave full screen so later steps see the timeline.
             window->setProperty("viewerOnly", false);
             session.seek(0);
             for (int i = 0; i < 3; ++i) press(Qt::Key_Right);
         }},
        {"half width", after(400), [&] {
             r.after_steps = session.position();
             r.stepped = std::abs(r.after_steps - 3.0 / session.frameRate()) < 1e-9;
             window->resize(820, 620);
         }},
        {"layout stress", after(100), [&] {
             // Repeated tiling-size changes and screen/popup switches must leave the editor
             // visible and its single GPU viewer attached to the same window.
             for (int i = 0; i < 24; ++i) {
                 window->resize(i % 2 == 0 ? 640 : 1600, i % 3 == 0 ? 520 : 900);
                 session.showProjects();
                 QTest::qWait(5);
                 session.continueProject();
                 actions.trigger(QStringLiteral("settings"));
                 QTest::qWait(5);
                 actions.trigger(QStringLiteral("settings"));
             }
             window->resize(1440, 900);
             QTest::qWait(20);
             bool stable = window->property("compact").toBool();
             for (int i = 0; i < 12; ++i) {
                 window->resize(i % 2 == 0 ? 1490 : 1510, 900);
                 QTest::qWait(5);
                 stable = stable && window->property("compact").toBool();
             }
             window->resize(1560, 900);
             QTest::qWait(20);
             stable = stable && !window->property("compact").toBool();
             for (int i = 0; i < 12; ++i) {
                 window->resize(i % 2 == 0 ? 1490 : 1510, 900);
                 QTest::qWait(5);
                 stable = stable && !window->property("compact").toBool();
             }
             window->resize(820, 620);
             QTest::qWait(100);
             auto* timeline = window->findChild<QQuickItem*>(QStringLiteral("timelinePanel"));
             auto* viewer = window->findChild<QQuickItem*>(QStringLiteral("preview"));
             const bool ok = stable && session.editing() && timeline && timeline->isVisible() &&
                             timeline->height() >= 120 && viewer && viewer->isVisible() &&
                             viewer->width() > 0 && viewer->height() > 0 &&
                             !window->grabWindow().isNull();
             r.layout = r.layout && ok;
             if (!ok) std::printf("GUI smoke: layout did not recover after repeated resize and screen switches\n");
         }},
        {"context menu", after(400), [&] {
             auto* timeline = window->findChild<QQuickItem*>(QStringLiteral("timelinePanel"));
             auto* upper = window->findChild<QQuickItem*>(QStringLiteral("upperWorkspace"));
             r.layout = r.layout && timeline && upper && upper->height() >= 200 && timeline->height() >= 120;
             if (!r.layout) std::printf("GUI smoke: library/viewer/timeline layout too small at half width\n");
             // Right click inside the first storyline clip; the next step picks "Split here".
             const auto clips = items_named("storylineClip");
             if (!clips.empty()) QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, center_of(clips.front()).toPoint());
         }},
        {"classic controls", after(300), [&] {
             const auto before = clip_count();
             const auto split_items = items_named("menuSplitHere");
             if (!split_items.empty()) QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center_of(split_items.front()).toPoint());
             const bool split = clip_count() == before + 1;
             if (!split) std::printf("GUI smoke: Split here from the context menu failed\n");
             if (split) session.undo();
             actions.trigger(QStringLiteral("clipMenu"));
             // Pointer access follows the same one-level branch as the keyboard check.
             QQuickItem* picture_item = nullptr;
             auto* storyline_menu = window->findChild<QObject*>(QStringLiteral("storylineMenu"));
             if (storyline_menu) {
                 QMetaObject::invokeMethod(storyline_menu, "itemAt", Q_RETURN_ARG(QQuickItem*, picture_item), Q_ARG(int, 0));
             }
             if (picture_item) QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center_of(picture_item).toPoint());
             // The storyline and lane menus each hold one; only the open submenu's is visible.
             // The submenu repositions itself after opening: click once its place is stable.
             const auto visible_color = [&] {
                 auto items = items_named("pictureColorItem");
                 items.removeIf([](const QQuickItem* i) { return !i->isVisible(); });
                 return items;
             };
             QPointF last(-1, -1);
             for (int stable = 0, waited = 0; stable < 3 && waited < 1000; waited += 20) {
                 QTest::qWait(20);
                 const auto items = visible_color();
                 const QPointF at = items.empty() ? QPointF(-1, -1) : center_of(items.front());
                 stable = at == last && !items.empty() ? stable + 1 : 0;
                 last = at;
             }
             // A user clicks what was drawn. grabWindow() polishes and syncs the scene as a frame
             // does (waiting for frameSwapped alone did not): clicking before that hit the item
             // below Color or nothing, though the drawn menu was right (checked on a capture).
             auto color_items = visible_color();
             if (!color_items.empty()) {
                 QTest::mouseMove(window, center_of(color_items.front()).toPoint());
                 (void)window->grabWindow();
                 color_items = visible_color();
             }
             if (!color_items.empty()) QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                                                       center_of(color_items.front()).toPoint());
             const bool color_from_menu = window->property("drawer").toString() == QStringLiteral("color");
             if (!color_from_menu) std::printf("GUI smoke: Picture > Color by pointer failed (branch %d, item %lld)\n",
                                               picture_item != nullptr, static_cast<long long>(color_items.size()));
             window->setProperty("drawer", QString());
             session.seek(0.5);
             session.togglePlay();
             const bool stop_clicked = click_button("transportStop");
             const bool stopped = stop_clicked && !session.playing() && session.position() == 0.0;
             const bool end_clicked = click_button("transportEnd");
             const bool at_end = end_clicked && session.position() > 0.0;
             const bool start_clicked = click_button("transportStart");
             r.controls = split && color_from_menu && stopped && at_end && start_clicked && session.position() == 0.0;
         }},
        {"discard guard", after(300), [&] {
             // With media in the session, New project asks before discarding anything.
             session.showProjects();
         }},
        {"tab", after(200), [&] {
             // Buttons take keyboard focus: from the top of Projects, Tab lands on Settings, then
             // Open project.
             window->contentItem()->forceActiveFocus();
             press(Qt::Key_Tab);
             const QQuickItem* first = window->activeFocusItem();
             r.tab = first != nullptr && first->objectName() == QStringLiteral("settingsButton");
             press(Qt::Key_Tab);
             const QQuickItem* focused = window->activeFocusItem();
             r.tab = r.tab && focused != nullptr && focused->objectName() == QStringLiteral("openProject");
         }},
        {"discard dialog", after(300), [&] {
             const auto before = session.media().size();
             const bool clicked = click_button("newProject");
             auto* dialog = window->findChild<QObject*>(QStringLiteral("discardDialog"));
             r.guard = clicked && dialog != nullptr && dialog->property("opened").toBool() && session.media().size() == before;
             screenshot(window, "OMA_GUI_SMOKE_DISCARD_SCREENSHOT");
             if (dialog != nullptr) QMetaObject::invokeMethod(dialog, "reject");
             session.continueProject();
             if (!r.guard) std::printf("GUI smoke: discard guard failed\n");
         }},
        {"save project", after(100), [&] {
             saved = {.media = session.media().size(), .clips = clip_count(), .lanes = lane_count(),
                      .duration = session.duration(), .first = session.clips().front().toMap(), .snapping = false, .font = {}, .next_font = {}, .launched = {}, .forwarded = {}, .menu_clip = 0, .undo_before_title = {}, .title_pixels = 0, .clips_after_edit = 0, .crashed = {}};
             r.project = session.dirty() && session.projectName() == QStringLiteral("Untitled project");
             session.saveProject(QUrl::fromLocalFile(project_file));
         }},
        {"saved", [&] { return !session.dirty() || session.failed(); }, [&] {
             r.project = r.project && !session.failed() && QFile::exists(project_file) &&
                         session.projectName() == QStringLiteral("smoke vlog");
             session.openProject(QUrl::fromLocalFile(project_file));
         }},
        {"reopened", [&] { return session.media().size() == saved.media || session.failed(); }, [&] {
             const QVariantMap first = session.clips().isEmpty() ? QVariantMap{} : session.clips().front().toMap();
             r.project = r.project && !session.failed() && !session.dirty() && clip_count() == saved.clips &&
                         lane_count() == saved.lanes && std::abs(session.duration() - saved.duration) < 1e-9 &&
                         first.value("start") == saved.first.value("start") &&
                         first.value("duration") == saved.first.value("duration") &&
                         first.value("name") == saved.first.value("name") &&
                         session.projectPath() == project_file;
             // Saving and opening put it on the Projects screen's recent list, once.
             const QVariantList recent = session.recentProjects();
             r.project = r.project && recent.size() == 1 &&
                         recent.front().toMap().value("path") == QFileInfo(project_file).absoluteFilePath();
             // ADR-0009: the reopened library came from the disk caches the first import filled.
             const auto entries = [&](const char* kind) {
                 const QDir dir(smoke_recordings.filePath(QStringLiteral("cache/omamovie/%1").arg(QLatin1String(kind))));
                 return dir.entryList({QStringLiteral("*.entry")}, QDir::Files).size();
             };
             r.project = r.project && entries("thumbnails") >= 2 && entries("waveforms") >= 1 &&
                         !session.media().isEmpty() &&
                         !session.media().front().toMap().value("thumbnail").toString().isEmpty();
             if (!r.project) std::printf("GUI smoke: caches hold %lld thumbnails, %lld waveforms\n",
                                         static_cast<long long>(entries("thumbnails")), static_cast<long long>(entries("waveforms")));
             if (!r.project) std::printf("GUI smoke: save and reopen failed (%s)\n", qPrintable(session.status()));
             // UX-01: Save in the discard dialog saves first, then does what was asked.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.setClipOpacity(0.5);
             session.showProjects();
         }},
        {"save from guard", after(300), [&] {
             const bool asked = click_button("newProject");
             const bool saved_first = asked && click_button("saveFirst");
             r.guard = r.guard && session.dirty() && saved_first;
             if (!r.guard) std::printf("GUI smoke: Save from guard failed (asked %d, clicked %d, dirty %d, importing %d, status '%s')\n",
                                       asked, saved_first, session.dirty(), session.importsPending(), qPrintable(session.status()));
         }},
        {"saved then new", [&] { return session.media().isEmpty() || run->elapsed() > 5000; }, [&] {
             r.guard = r.guard && session.media().isEmpty() && session.projectPath().isEmpty();
             if (!r.guard) std::printf("GUI smoke: Save from guard did not make a new project (media %lld, path '%s', dirty %d, status '%s')\n",
                                       static_cast<long long>(session.media().size()), qPrintable(session.projectPath()),
                                       session.dirty(), qPrintable(session.status()));
             session.openProject(QUrl::fromLocalFile(project_file));
         }},
        {"guard save kept", [&] { return session.media().size() == saved.media || session.failed(); }, [&] {
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             const bool kept = session.info().value("opacity").toDouble() == 0.5;
             if (!kept) std::printf("GUI smoke: Save in the discard dialog did not save the edit\n");
             r.guard = r.guard && kept;
             session.setClipOpacity(1.0); // back to what the later steps expect
             session.saveProject(QUrl());
         }},
        {"guard restored", [&] { return !session.dirty() || session.failed(); }, [&] {
             // Mistaken-edit recovery (M7): the file this save replaced is an earlier version,
             // and it opens as unsaved changes to the same project.
             const QVariantList versions = session.projectVersions(project_file);
             saved.relinked = false; // reused below as "a version was offered"
             if (versions.isEmpty()) {
                 r.project = false;
                 std::printf("GUI smoke: no earlier versions kept\n");
                 return;
             }
             saved.relinked = true;
             session.restoreVersion(versions.front().toMap().value("file").toString(), project_file);
         }},
        {"version opened", [&] { return (session.media().size() == saved.media && !session.importsPending()) || session.failed(); }, [&] {
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             const bool ok = saved.relinked && session.dirty() && session.projectPath() == project_file &&
                             session.info().value("opacity").toDouble() == 0.5 && session.projectVersions(project_file).size() >= 1;
             if (!ok) std::printf("GUI smoke: earlier version not restored (dirty %d, path %s, opacity %.2f)\n", session.dirty(),
                                  qPrintable(session.projectPath()), session.info().value("opacity").toDouble());
             r.project = r.project && ok;
             session.openProject(QUrl::fromLocalFile(project_file)); // back to the saved file
         }},
        {"version left", [&] { return (session.media().size() == saved.media && !session.importsPending() && !session.dirty()) ||
                                      session.failed() || run->elapsed() > 5000; }, [&] {
             // Retention: twelve saves of one file keep the newest versions::kKeep.
             const QString file = project_dir.filePath(QStringLiteral("retention.omamovie"));
             QFile out(file);
             const bool written = out.open(QIODevice::WriteOnly) && out.write("{}") == 2;
             out.close();
             for (int i = 0; i < 12; ++i) {
                 (void)versions::archive(file);
                 QThread::msleep(2); // versions are named by the millisecond
             }
             const bool kept = written && versions::list(file).size() == versions::kKeep;
             if (!kept) std::printf("GUI smoke: version retention kept %zu\n", versions::list(file).size());
             r.project = r.project && kept;
         }},
        {"wide window", after(100), [&] {
             window->resize(1600, 900);
         }},
        {"hide library", after(400), [&] {
             // Wide windows fold the library sidebar away and give the viewer the whole width.
             auto* sidebar = window->findChild<QQuickItem*>(QStringLiteral("librarySidebar"));
             auto* viewer = window->findChild<QQuickItem*>(QStringLiteral("viewerColumn"));
             auto* divider = window->findChild<QQuickItem*>(QStringLiteral("libraryDivider"));
             const bool shown = sidebar != nullptr && sidebar->isVisible() && viewer != nullptr && viewer->x() > 0;
             const double before = sidebar ? sidebar->width() : 0;
             if (divider && divider->isVisible()) {
                 const QPointF from = center_of(divider);
                 const QPointF to = from + QPointF(100, 0);
                 QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from.toPoint());
                 for (int i = 1; i <= 8; ++i) QTest::mouseMove(window, (from + (to - from) * i / 8.0).toPoint(), 10);
                 QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to.toPoint(), 10);
             }
             const bool resized = sidebar && sidebar->width() > before + 50 && viewer->x() == sidebar->width();
             const bool clicked = click_button("toggleLibrary");
             const bool hidden = clicked && !sidebar->isVisible() && viewer->x() == 0.0 &&
                                 viewer->width() == window->contentItem()->width();
             screenshot(window, "OMA_GUI_SMOKE_WIDE_SCREENSHOT");
             window->setProperty("libraryHidden", false);
             r.project = r.project && shown && resized && hidden;
             if (!(shown && resized && hidden)) std::printf("GUI smoke: resizing or hiding the library failed\n");
             window->resize(820, 620); // back to the size of the steps before
         }},
        {"library", after(500), [&] {
             window->setProperty("libraryOverlay", true);
         }},
        {"recordings source", after(300), [&] {
             saved.media = session.media().size();
             const bool clicked = click_button("recordingsSource");
             if (!clicked) std::printf("GUI smoke: recordings source button unavailable\n");
             r.recordings = r.recordings && clicked;
         }},
        {"import recording", after(300), [&] {
             const auto items = items_named("importRecording");
             if (items.size() != 1) std::printf("GUI smoke: %lld import buttons for recordings\n", static_cast<long long>(items.size()));
             r.recordings = r.recordings && items.size() == 1;
             // The button's own click: the library overlay may still be sliding in, so a click at
             // its coordinates can land beside it.
             if (!items.isEmpty()) QMetaObject::invokeMethod(items.front(), "click");
         }},
        {"recording imported", [&] { return session.media().size() > saved.media || run->elapsed() > 5000; }, [&] {
             r.recordings = r.recordings && session.media().size() == saved.media + 1 &&
                            window->property("librarySource").toString() == QStringLiteral("project");
             if (!r.recordings) std::printf("GUI smoke: importing a recording from the library failed (media %lld -> %lld, source %s, window %dx%d)\n",
                                            static_cast<long long>(saved.media), static_cast<long long>(session.media().size()),
                                            qPrintable(window->property("librarySource").toString()), window->width(), window->height());
         }},
        {"connect", after(200), [&] {
             // Q: the selected video above the storyline at the playhead, connected (ADR-0014).
             window->setProperty("libraryOverlay", false);
             session.selectMedia(static_cast<int>(session.media().size()) - 1);
             session.seek(0.3);
             session.connectSelected();
             const QVariantList lanes = session.videoTracks();
             const QVariantList clips = lanes.isEmpty() ? QVariantList{} : lanes.front().toMap().value("clips").toList();
             r.connected = lanes.size() == 1 && clips.size() == 1 && clips.front().toMap().value("connected").toBool() &&
                           session.undoText() == QStringLiteral("Connect");
             if (!r.connected) std::printf("GUI smoke: connect failed (%lld lanes, undo '%s', media %lld, notice '%s')\n",
                                           static_cast<long long>(lanes.size()), qPrintable(session.undoText()),
                                           static_cast<long long>(session.media().size()), qPrintable(session.status()));
         }},
        {"connected drawn", after(300), [&] {
             r.connected = r.connected && items_named("overlayClip").size() == 1;
             session.undo();
             r.connected = r.connected && session.videoTracks().isEmpty();
             session.redo();
             r.connected = r.connected && session.videoTracks().size() == 1;
             // The lane clip's menu ends and remakes the connection.
             const QVariantList tracks = session.videoTracks();
             const QVariantList lane_clips = tracks.isEmpty() ? QVariantList{} : tracks.front().toMap().value("clips").toList();
             const double overlay = lane_clips.isEmpty() ? 0.0 : lane_clips.front().toMap().value("id").toDouble();
             session.selectClip(overlay);
             session.disconnectSelectedClip();
             r.connected = r.connected && !session.info().value("connected").toBool();
             session.connectSelectedClip();
             r.connected = r.connected && session.info().value("connected").toBool() &&
                           session.undoText() == QStringLiteral("Connect");
             if (!r.connected) std::printf("GUI smoke: connecting a clip above the storyline failed\n");
         }},
        {"followed outline", after(200), [&] {
             // The selected connected clip's storyline clip carries the outline, and only it; hovering
             // the lane clip shows the same one.
             const auto outlined = [&] {
                 int n = 0;
                 double id = -1;
                 for (QQuickItem* c : items_named("storylineClip")) {
                     if (c->property("followed").toBool()) {
                         ++n;
                         id = c->property("clipId").toDouble();
                     }
                 }
                 return n == 1 ? id : -2.0;
             };
             const double primary = session.info().value("primary").toDouble();
             const double selected_outline = outlined();
             r.connected = r.connected && primary > 0 && selected_outline == primary;
             const auto lane = items_named("overlayClip");
             if (!lane.isEmpty()) QTest::mouseMove(window, center_of(lane.front()).toPoint());
             session.selectClip(0); // nothing selected: only the hover can draw the outline
             const double hover_outline = outlined();
             r.connected = r.connected && !session.info().value("connected").toBool() && hover_outline == primary;
             if (!r.connected) std::printf("GUI smoke: followed outline wrong (primary %.0f, selected %.0f, hovered %.0f, %lld lane clips)\n",
                                           primary, selected_outline, hover_outline, static_cast<long long>(lane.size()));
             QTest::mouseMove(window, QPoint(2, 2));
         }},
        // Alt+Shift+dragging a lane clip slides it (LaneDrag): one Slide entry, undone by one undo.
        {"lane slide", after(100), [&] {
             const auto lane_start = [&] {
                 const QVariantList tracks = session.videoTracks();
                 const QVariantList clips = tracks.isEmpty() ? QVariantList{} : tracks.front().toMap().value("clips").toList();
                 return clips.isEmpty() ? -1.0 : clips.front().toMap().value("start").toDouble();
             };
             const auto lane = items_named("overlayClip");
             const double before = lane_start();
             if (!lane.isEmpty()) {
                 const QPointF from = center_of(lane.front());
                 const QPointF to = from + QPointF(40, 0);
                 const auto mods = Qt::AltModifier | Qt::ShiftModifier;
                 QTest::mousePress(window, Qt::LeftButton, mods, from.toPoint());
                 for (int i = 1; i <= 8; ++i) QTest::mouseMove(window, (from + (to - from) * i / 8.0).toPoint(), 10);
                 QTest::mouseRelease(window, Qt::LeftButton, mods, to.toPoint(), 10);
             }
             const double slid = lane_start();
             const bool entry = session.undoText() == QStringLiteral("Slide");
             session.undo();
             const bool ok = !lane.isEmpty() && slid > before && entry && lane_start() == before;
             if (!ok) std::printf("GUI smoke: lane slide failed (%lld lane clips, start %.3f -> %.3f, undo '%s')\n",
                                  static_cast<long long>(lane.size()), before, slid, qPrintable(session.undoText()));
             r.connected = r.connected && ok;
             QTest::mouseMove(window, QPoint(2, 2));
         }},
        // Titles (ADR-0015): Add title puts one at the playhead above the storyline, connected,
        // opens its drawer, and the viewer shows yellow text in the lower third once recoloured.
        {"add title", after(100), [&] {
             session.seek(0.2);
             saved.undo_before_title = session.undoText();
             actions.trigger(QStringLiteral("addTitle"));
             const QVariantMap info = session.info();
             r.titles = info.value("isTitle").toBool() && info.value("connected").toBool() &&
                        session.undoText() == QStringLiteral("Add Title") &&
                        window->property("drawer").toString() == QStringLiteral("title");
             session.setClipTitle(QStringLiteral("Hello OmaMovie"), 0.2, QStringLiteral("#ffd400"), 0);
             r.titles = r.titles && session.info().value("titleText") == QStringLiteral("Hello OmaMovie") &&
                        session.undoText() == QStringLiteral("Title");
             if (!r.titles) std::printf("GUI smoke: adding a title failed (%s)\n", qPrintable(session.status()));
         }},
        {"title drawer", after(300), [&] {
             r.titles = r.titles && items_named("titleDrawer").size() == 1;
             actions.trigger(QStringLiteral("clipMenu"));
             auto* menu = window->findChild<QObject*>(QStringLiteral("titleMenu"));
             auto* title_item = window->findChild<QObject*>(QStringLiteral("overlayTitleItem"));
             const bool title_menu = menu && menu->property("opened").toBool() && title_item &&
                                     title_item->property("visible").toBool() && menu->property("count").toInt() == 6;
             r.titles = r.titles && title_menu;
             if (!title_menu) std::printf("GUI smoke: title menu has wrong entries (%d)\n",
                                          menu ? menu->property("count").toInt() : -1);
             if (menu) QMetaObject::invokeMethod(menu, "close");
             window->setProperty("drawer", QString()); // the same viewer size for both measurements
         }},
        {"title drawn", after(600), [&] {
             saved.title_pixels = yellow_in_lower_viewer();
             screenshot(window, "OMA_GUI_SMOKE_TITLE_SCREENSHOT");
             session.undo(); // the text
             session.undo(); // the title
             r.titles = r.titles && session.undoText() == saved.undo_before_title && !session.info().value("isTitle").toBool();
             window->setProperty("drawer", QString());
         }},
        {"title gone", after(600), [&] {
             // The fixture has yellow bars of its own: the title must add yellow, not just find it.
             const int without = yellow_in_lower_viewer();
             r.titles = r.titles && saved.title_pixels > without + 20;
             if (!r.titles) std::printf("GUI smoke: title not drawn (%d yellow samples with it, %d without)\n",
                                        saved.title_pixels, without);
         }},
        // Menus by keyboard (ui-design §7.3): Shift+F10 opens the selected clip's menu with the
        // focus in it; Down/Right/Down/Enter traverse the picture branch, Escape closes it.
        {"menu keys", after(100), [&] {
             saved.menu_clip = session.clips().front().toMap().value("id").toDouble();
             session.selectClip(saved.menu_clip);
             window->setProperty("drawer", QString());
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"menu focused", after(300), [&] {
             auto* menu = window->findChild<QObject*>(QStringLiteral("storylineMenu"));
             const QQuickItem* focus = window->activeFocusItem();
             auto* content = menu ? menu->property("contentItem").value<QQuickItem*>() : nullptr;
             bool inside = false;
             for (const QQuickItem* i = focus; i != nullptr && content != nullptr; i = i->parentItem()) inside = inside || i == content;
             r.menu_keys = menu && menu->property("opened").toBool() && inside;
             auto* picture = window->findChild<QObject*>(QStringLiteral("storylinePictureMenu"));
             auto* sound = window->findChild<QObject*>(QStringLiteral("storylineSoundMenu"));
             r.menu_keys = r.menu_keys && picture && picture->property("count").toInt() == 3 &&
                           (sound != nullptr) == session.info().value("hasAudio").toBool() &&
                           !window->findChild<QObject*>(QStringLiteral("pictureTransformItem")) &&
                           !window->findChild<QObject*>(QStringLiteral("pictureGradeItem")) &&
                           window->findChild<QObject*>(QStringLiteral("soundEqItem")) &&
                           window->findChild<QObject*>(QStringLiteral("soundNoiseItem"));
             if (!r.menu_keys) std::printf("GUI smoke: clip menu %s, focus %s\n", menu && menu->property("opened").toBool() ? "open" : "closed",
                                           focus ? focus->metaObject()->className() : "none");
             press(Qt::Key_Down);
             press(Qt::Key_Right);
             press(Qt::Key_Down);
             press(Qt::Key_Return);
         }},
        {"menu ran", after(300), [&] {
             auto* menu = window->findChild<QObject*>(QStringLiteral("storylineMenu"));
             r.menu_keys = r.menu_keys && !menu->property("opened").toBool() &&
                           window->property("drawer").toString() == QStringLiteral("color") &&
                           session.selectedClip() == saved.menu_clip;
             if (!r.menu_keys) std::printf("GUI smoke: after Enter: menu %d, drawer %s, selected %.0f vs %.0f\n", menu->property("opened").toBool(),
                                           qPrintable(window->property("drawer").toString()), session.selectedClip(), saved.menu_clip);
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"menu escape", after(300), [&] {
             press(Qt::Key_Escape);
         }},
        {"menu closed", after(250), [&] {
             auto* menu = window->findChild<QObject*>(QStringLiteral("storylineMenu"));
             r.menu_keys = r.menu_keys && !menu->property("opened").toBool() &&
                           window->property("drawer").toString() == QStringLiteral("color");
             if (!r.menu_keys) std::printf("GUI smoke: menu keyboard check failed (drawer '%s')\n",
                                           qPrintable(window->property("drawer").toString()));
             window->setProperty("drawer", QString());
             actions.trigger(QStringLiteral("transformControls"));
         }},
        {"precise picture", after(150), [&] {
             const QQuickItem* focus = window->activeFocusItem();
             const bool ok = window->property("drawer").toString() == QStringLiteral("crop") &&
                             window->property("cropMore").toBool() && focus &&
                             focus->objectName() == QStringLiteral("drawerSlider:POSITION X") &&
                             session.selectedClip() == saved.menu_clip;
             if (!ok) std::printf("GUI smoke: transform controls did not open with focus\n");
             r.menu_keys = r.menu_keys && ok;
             actions.trigger(QStringLiteral("colorGrade"));
         }},
        {"precise color", after(150), [&] {
             const QQuickItem* focus = window->activeFocusItem();
             const bool ok = window->property("drawer").toString() == QStringLiteral("color") &&
                             window->property("colorMore").toBool() && focus &&
                             focus->objectName().startsWith(QStringLiteral("gradeTab:"));
             if (!ok) std::printf("GUI smoke: color grading did not open with focus\n");
             r.menu_keys = r.menu_keys && ok;
             // The same walk on an audio lane clip's menu.
             const QVariantList lanes = session.audioTracks();
             const QVariantList sounds = lanes.isEmpty() ? QVariantList{} : lanes.front().toMap().value("clips").toList();
             if (sounds.isEmpty()) {
                 r.menu_keys = false;
                 std::printf("GUI smoke: no audio lane clip for the menu walk\n");
                 return;
             }
             session.selectClip(sounds.front().toMap().value("id").toDouble());
             actions.trigger(QStringLiteral("equalizer"));
         }},
        {"precise sound", after(150), [&] {
             const QQuickItem* focus = window->activeFocusItem();
             const bool eq = window->property("drawer").toString() == QStringLiteral("volume") &&
                             window->property("volumeMore").toBool() && focus &&
                             focus->objectName() == QStringLiteral("audioEqPreset");
             actions.trigger(QStringLiteral("noiseReduction"));
             r.menu_keys = r.menu_keys && eq;
             if (!eq) std::printf("GUI smoke: equalizer did not open with focus\n");
         }},
        {"precise noise", after(150), [&] {
             const QQuickItem* focus = window->activeFocusItem();
             const bool ok = focus && focus->objectName() == QStringLiteral("drawerSlider:NOISE REDUCTION") &&
                             session.info().value("hasAudio").toBool();
             if (!ok) std::printf("GUI smoke: noise reduction did not open with focus\n");
             r.menu_keys = r.menu_keys && ok;
             saved.menu_audio_muted = session.info().value("muted").toBool();
             actions.trigger(QStringLiteral("toggleMute"));
         }},
        {"sound mute", after(150), [&] {
             const bool muted = session.info().value("muted").toBool() != saved.menu_audio_muted;
             const bool label = window->findChild<QObject*>(QStringLiteral("soundMuteItem")) != nullptr;
             if (!muted || !label) std::printf("GUI smoke: sound mute action did not update\n");
             r.menu_keys = r.menu_keys && muted && label;
             actions.trigger(QStringLiteral("toggleMute"));
             window->setProperty("drawer", QString());
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"lane menu", after(300), [&] {
             auto* menu = window->findChild<QObject*>(QStringLiteral("soundMenu"));
             const bool ok = menu_has_focus(QStringLiteral("soundMenu")) &&
                             menu && menu->property("count").toInt() == 6 &&
                             window->findChild<QObject*>(QStringLiteral("laneSoundMenu")) &&
                             !window->findChild<QObject*>(QStringLiteral("lanePictureMenu"));
             if (!ok) std::printf("GUI smoke: audio lane menu not open with focus\n");
             r.menu_keys = r.menu_keys && ok;
             press(Qt::Key_Escape);
         }},
        {"overlay menu", after(250), [&] {
             const QVariantList tracks = session.videoTracks();
             const QVariantList clips = tracks.isEmpty() ? QVariantList{} : tracks.front().toMap().value("clips").toList();
             if (clips.isEmpty()) { r.menu_keys = false; return; }
             session.selectClip(clips.front().toMap().value("id").toDouble());
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"overlay tree", after(300), [&] {
             const bool ok = menu_has_focus(QStringLiteral("overlayMenu")) &&
                             window->findChild<QObject*>(QStringLiteral("overlayPictureMenu")) &&
                             (window->findChild<QObject*>(QStringLiteral("overlaySoundMenu")) != nullptr) ==
                                 session.info().value("hasAudio").toBool();
             if (!ok) std::printf("GUI smoke: overlay menu has wrong branches\n");
             r.menu_keys = r.menu_keys && ok;
             // Keyboard activation of the menu's Delete: it must remove this lane clip, not the
             // storyline clip, in one undoable step.
             auto* menu = window->findChild<QObject*>(QStringLiteral("overlayMenu"));
             const int count = menu ? menu->property("count").toInt() : 0;
             int target = -1;
             for (int i = 0; i < count; ++i) {
                 QQuickItem* item = nullptr;
                 QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, i));
                 if (item && item->property("text").toString() == QStringLiteral("Delete")) target = i;
             }
             saved.overlay_clips = 0;
             for (const QVariant& lane : session.videoTracks()) saved.overlay_clips += lane.toMap().value("clips").toList().size();
             saved.storyline_clips = session.clips().size();
             if (target < 0) { r.menu_keys = false; std::printf("GUI smoke: overlay menu has no Delete\n"); press(Qt::Key_Escape); return; }
             menu->setProperty("currentIndex", target);
             press(Qt::Key_Return);
         }},
        {"overlay delete", after(250), [&] {
             const auto lanes = [&] {
                 qsizetype n = 0;
                 for (const QVariant& lane : session.videoTracks()) n += lane.toMap().value("clips").toList().size();
                 return n;
             };
             const bool removed = lanes() + 1 == saved.overlay_clips && session.clips().size() == saved.storyline_clips;
             session.undo();
             const bool undone = lanes() == saved.overlay_clips;
             session.redo();
             const bool redone = lanes() + 1 == saved.overlay_clips;
             session.undo();
             const bool ok = removed && undone && redone && lanes() == saved.overlay_clips;
             if (!ok) std::printf("GUI smoke: overlay menu Delete (removed %d, undo %d, redo %d)\n", removed, undone, redone);
             r.menu_keys = r.menu_keys && ok;
         }},
        {"cut menu", after(250), [&] {
             // Nothing selected and the playhead on a cut: the cut's transition menu.
             r.menu_keys = r.menu_keys && !window->findChild<QObject*>(QStringLiteral("soundMenu"))->property("opened").toBool();
             actions.trigger(QStringLiteral("deselect"));
             session.seek(session.clips().value(1).toMap().value("start").toDouble());
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"cut menu open", after(300), [&] {
             const bool ok = session.selectedClip() == 0 && menu_has_focus(QStringLiteral("transitionMenu"));
             if (!ok) std::printf("GUI smoke: cut menu not open with focus (selected %.0f)\n", session.selectedClip());
             r.menu_keys = r.menu_keys && ok;
             press(Qt::Key_Escape);
         }},
        {"empty menu", after(250), [&] {
             // Away from any cut: the timeline's menu; Enter runs its first item.
             const QVariantMap first = session.clips().front().toMap();
             session.seek(first.value("start").toDouble() + first.value("duration").toDouble() / 2);
             actions.trigger(QStringLiteral("clipMenu"));
         }},
        {"empty menu open", after(300), [&] {
             const bool ok = menu_has_focus(QStringLiteral("emptyTimelineMenu"));
             if (!ok) std::printf("GUI smoke: timeline menu not open with focus\n");
             r.menu_keys = r.menu_keys && ok;
             press(Qt::Key_Down);
             press(Qt::Key_Return);
         }},
        {"empty menu ran", after(300), [&] {
             r.menu_keys = r.menu_keys && !window->findChild<QObject*>(QStringLiteral("emptyTimelineMenu"))->property("opened").toBool();
         }},
        // Ripple scope (M6): a pending delete or trim shows who moves and who goes, simulated with
        // the very command the edit runs; nothing changes until the edit happens.
        {"edit scope", after(100), [&] {
             const QVariantList clips = session.clips();
             bool ok = clips.size() >= 2;
             if (ok) {
                 const QString first = clips.at(0).toMap().value("id").toString();
                 const QString second = clips.at(1).toMap().value("id").toString();
                 const auto undo_before = session.undoText();
                 session.selectClip(clips.at(0).toMap().value("id").toDouble());
                 session.previewDelete(true);
                 QVariantMap scope = session.editScope();
                 ok = scope.value("removed").toList().contains(clips.at(0).toMap().value("id")) &&
                      scope.value("moved").toMap().value(second).toInt() == -1 &&
                      !scope.value("moved").toMap().contains(first) && clip_count() == clips.size() &&
                      session.undoText() == undo_before;
                 screenshot(window, "OMA_GUI_SMOKE_SCOPE_SCREENSHOT");
                 session.previewTrim(clips.at(0).toMap().value("id").toDouble(), false, -5);
                 scope = session.editScope();
                 ok = ok && scope.value("removed").toList().isEmpty() && scope.value("moved").toMap().value(second).toInt() == -1;
                 session.clearEditScope();
                 ok = ok && !session.editScope().value("active").toBool();
             }
             if (!ok) std::printf("GUI smoke: edit scope preview failed\n");
             r.lanes = r.lanes && ok;
         }},
        // Canvas proportion (ADR-0010): a vertical canvas shows in the viewer, and undo restores it.
        {"vertical canvas", after(100), [&] {
             saved.canvas_before = session.canvasWidth();
             session.setCanvasAspect(9, 16);
         }},
        {"vertical shown", after(500), [&] {
             screenshot(window, "OMA_GUI_SMOKE_CANVAS_SCREENSHOT");
             const bool vertical = session.canvasHeight() > session.canvasWidth();
             session.undo();
             const bool restored = session.canvasWidth() == saved.canvas_before;
             if (!(vertical && restored)) std::printf("GUI smoke: canvas proportion failed\n");
             r.lanes = r.lanes && vertical && restored;
         }},
        // Custom size: the dialog opens with the current size, and OK applies the typed one.
        {"canvas dialog", after(100), [&] {
             actions.trigger(QStringLiteral("canvasCustom"));
         }},
        {"canvas dialog shown", after(300), [&] {
             auto* dialog = window->findChild<QObject*>(QStringLiteral("canvasDialog"));
             auto* w = window->findChild<QObject*>(QStringLiteral("canvasWidthBox"));
             auto* h = window->findChild<QObject*>(QStringLiteral("canvasHeightBox"));
             bool ok = dialog && w && h && dialog->property("opened").toBool() &&
                       w->property("value").toInt() == session.canvasWidth();
             if (ok) {
                 w->setProperty("value", 400);
                 h->setProperty("value", 400);
                 QMetaObject::invokeMethod(dialog, "accept");
                 ok = session.canvasWidth() == 400 && session.canvasHeight() == 400;
                 session.undo();
                 ok = ok && session.canvasWidth() == saved.canvas_before;
             }
             if (!ok) std::printf("GUI smoke: canvas size dialog failed (%dx%d)\n", session.canvasWidth(), session.canvasHeight());
             r.lanes = r.lanes && ok;
         }},
        {"framing gesture", after(200), [&] {
             // UX-07: with Crop & framing open, dragging the box in the viewer moves the picture;
             // the gesture is one undo entry.
             const QVariantMap first = session.clips().front().toMap();
             session.selectClip(first.value("id").toDouble());
             session.seek(first.value("start").toDouble() + 0.1);
             window->setProperty("drawer", QStringLiteral("crop"));
         }},
        {"framing drag", after(400), [&] {
             const auto handles = items_named("framingMove");
             const double before = session.motion().value("posX").toDouble();
             const QString undo_before = session.undoText();
             bool ok = handles.size() == 1;
             if (ok) {
                 const QPointF from = center_of(handles.front());
                 QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from.toPoint());
                 for (int i = 1; i <= 8; ++i) QTest::mouseMove(window, (from + QPointF(5.0 * i, 0)).toPoint(), 10);
                 QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, (from + QPointF(40, 0)).toPoint(), 10);
                 const double after_drag = session.motion().value("posX").toDouble();
                 ok = after_drag > before + 10 && session.undoText() != undo_before;
                 session.undo();
                 ok = ok && session.motion().value("posX").toDouble() == before && session.undoText() == undo_before;
             }
             // The left crop handle: dragged inward, it crops the source's left edge in one edit.
             const auto crops = items_named("cropHandle");
             bool cropped = crops.size() == 4;
             if (cropped) {
                 const QPointF from = center_of(crops.front());
                 drag_mouse(from, from + QPointF(30, 0));
                 const double left = session.info().value("cropLeft").toDouble();
                 cropped = left > 0.01 && left <= 0.45 && session.info().value("cropRight").toDouble() == 0.0 &&
                           session.undoText() != undo_before;
                 if (!cropped) std::printf("GUI smoke: crop handle gave left %.3f\n", left);
                 session.undo();
                 cropped = cropped && session.info().value("cropLeft").toDouble() == 0.0 && session.undoText() == undo_before;
             } else {
                 std::printf("GUI smoke: %lld crop handles\n", static_cast<long long>(crops.size()));
             }
             ok = ok && cropped;
             // The rotate knob: a quarter turn clockwise around the box's center, one edit.
             const auto knobs = items_named("framingRotate");
             const auto boxes = items_named("layerBoxRect");
             bool turned = knobs.size() == 1 && boxes.size() == 1;
             if (turned) {
                 const QPointF knob = center_of(knobs.front());
                 const QPointF center = center_of(boxes.front());
                 const double radius = std::hypot(knob.x() - center.x(), knob.y() - center.y());
                 QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, knob.toPoint());
                 for (int i = 1; i <= 12; ++i) {
                     const double a = -M_PI / 2 + (M_PI / 2) * i / 12.0; // from straight up to the right
                     QTest::mouseMove(window, (center + QPointF(std::cos(a), std::sin(a)) * radius).toPoint(), 10);
                 }
                 QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, (center + QPointF(radius, 0)).toPoint(), 10);
                 const double rotation = session.motion().value("rotation").toDouble();
                 turned = std::abs(rotation - 90.0) < 2.0 && session.undoText() != undo_before;
                 if (!turned) std::printf("GUI smoke: rotate knob gave %.1f degrees\n", rotation);
                 session.undo();
                 turned = turned && session.motion().value("rotation").toDouble() == 0.0 && session.undoText() == undo_before;
             } else {
                 std::printf("GUI smoke: rotate knob missing\n");
             }
             // Crop handles on a turned clip: an edge's handle maps back to that edge's crop, and
             // moving it toward the center crops more (the geometry includes the rotation).
             session.setClipTransform(0, 0, 1, 90);
             const QVariantMap box = session.cropBox(0, 0, 0, 0);
             const QVariantMap at = box.value("left").toMap();
             const double cx = box.value("x").toDouble() + box.value("w").toDouble() / 2;
             const double cy = box.value("y").toDouble() + box.value("h").toDouble() / 2;
             const double px = at.value("x").toDouble();
             const double py = at.value("y").toDouble();
             const double same = session.cropEdgeAt(0, px, py, 0, 0, 0, 0);
             const double inward = session.cropEdgeAt(0, px + (cx - px) * 0.2, py + (cy - py) * 0.2, 0, 0, 0, 0);
             // Turned 90° clockwise, the source's left edge is at the top of the canvas.
             const bool rotated_crop = !box.isEmpty() && std::abs(same) < 0.002 && std::abs(inward - 0.1) < 0.01 && py < cy - 0.01;
             if (!rotated_crop) std::printf("GUI smoke: rotated crop mapping %.3f %.3f\n", same, inward);
             session.undo();
             ok = ok && turned && rotated_crop;
             window->setProperty("drawer", QString());
             r.timing = ok;
             if (!ok) std::printf("GUI smoke: framing gesture failed (%lld handles)\n", static_cast<long long>(handles.size()));
         }},
        {"freeze and reverse", after(200), [&] {
             // ADR-0013 from the clip menu: a two-second freeze at the playhead, then reverse.
             const QVariantMap first = session.clips().front().toMap();
             const double before = session.duration();
             session.selectClip(first.value("id").toDouble());
             session.seek(first.value("start").toDouble() + 0.2);
             session.freezeFrame(2.0);
             const QVariantMap frozen = session.clips().front().toMap();
             r.timing = r.timing && std::abs(frozen.value("duration").toDouble() - first.value("duration").toDouble() - 2.0) < 1e-6 &&
                        frozen.value("timing").toString() == QStringLiteral("Freeze");
             session.reverseClip();
             r.timing = r.timing && session.clips().front().toMap().value("timing").toString().startsWith("Reverse");
             session.undo();
             session.undo();
             r.timing = r.timing && std::abs(session.duration() - before) < 1e-6 &&
                        session.clips().front().toMap().value("timing").toString().isEmpty();
             // Opacity from the framing drawer marks the clip as adjusted; undo clears both.
             session.selectClip(first.value("id").toDouble());
             const bool plain = !session.clips().front().toMap().value("adjusted").toBool() ||
                                session.info().value("opacity").toDouble() == 1.0;
             session.setClipOpacity(0.5);
             const bool faded = session.info().value("opacity").toDouble() == 0.5 &&
                                session.clips().front().toMap().value("adjusted").toBool() &&
                                session.info().value("framingAdjusted").toBool();
             session.undo();
             r.timing = r.timing && plain && faded && session.info().value("opacity").toDouble() == 1.0;
             if (!r.timing) std::printf("GUI smoke: freeze/reverse/opacity failed (%s)\n", qPrintable(session.status()));
         }},
        // Command palette and remapping (ui-design §8): search, conflicts, a remapped key firing,
        // and reset. Overrides go to the smoke run's own settings file.
        {"palette", after(200), [&] {
             const QVariantList hits = actions.search(QStringLiteral("snap"));
             r.palette = !hits.isEmpty() && hits.front().toMap().value("id") == QStringLiteral("snapping") &&
                         actions.search(QStringLiteral(":split")).value(0).toMap().value("id") == QStringLiteral("split") &&
                         actions.search(QStringLiteral(":speed 2x")).value(0).toMap().value("id") == QStringLiteral("speedDouble") &&
                         actions.search(QStringLiteral(":normalize-audio")).value(0).toMap().value("id") == QStringLiteral("normalizeAudio") &&
                         actions.search(QString()).size() == actions.actions().size() &&
                         // A free key, spelled in two cases (M is the marker shortcut).
                         actions.remap(QStringLiteral("snapping"), QStringLiteral("f8")).isEmpty() &&
                         !actions.remap(QStringLiteral("zoomFit"), QStringLiteral("F8")).isEmpty() &&
                         !actions.remap(QStringLiteral("snapping"), QStringLiteral("Space")).isEmpty();
             // Default shortcuts: only Escape is shared, by two actions never enabled together.
             const QStringList shared = actions.conflicts();
             // Escape is shared by actions never enabled together (source viewer, drawer, full viewer).
             r.palette = r.palette && shared == QStringList{QStringLiteral("closeSource closeDrawer"),
                                                            QStringLiteral("closeSource leaveFullViewer"),
                                                            QStringLiteral("closeDrawer leaveFullViewer")};
             if (!r.palette) std::printf("GUI smoke: palette, remap or shortcut conflicts failed (%s)\n",
                                         qPrintable(shared.join(QStringLiteral(", "))));
             actions.trigger(QStringLiteral("snapping"));
             r.palette = r.palette && actions.search(QString()).value(0).toMap().value("id") == QStringLiteral("snapping");
             actions.trigger(QStringLiteral("snapping"));
             saved.snapping = window->property("snapping").toBool();
             actions.trigger(QStringLiteral("commandPalette"));
         }},
        {"palette typed", after(300), [&] {
             auto* palette = window->findChild<QObject*>(QStringLiteral("commandPalette"));
             r.palette = r.palette && palette && palette->property("opened").toBool();
             r.keys_focused = r.keys_focused && QGuiApplication::focusWindow() == window;
             for (const Qt::Key key : {Qt::Key_S, Qt::Key_N, Qt::Key_A, Qt::Key_P}) press(key);
             press(Qt::Key_Return);
         }},
        {"palette ran", after(300), [&] {
             auto* palette = window->findChild<QObject*>(QStringLiteral("commandPalette"));
             r.palette_keys = window->property("snapping").toBool() != saved.snapping && palette &&
                              !palette->property("opened").toBool();
             press(Qt::Key_F8); // the remapped shortcut toggles it back
         }},
        {"remapped key", after(250), [&] {
             r.palette_keys = r.palette_keys && window->property("snapping").toBool() == saved.snapping;
             actions.resetAll();
             const QVariantList hits = actions.search(QStringLiteral("snapping"));
             r.palette = r.palette && !hits.isEmpty() && hits.front().toMap().value("keys") == QStringLiteral("N");
         }},
        // Settings (M6): live preview quality and recordings folder, decode path for the next
        // start, persistence across instances, invalid values falling back, reset.
        {"settings", after(100), [&] {
             actions.trigger(QStringLiteral("settings"));
         }},
        {"settings open", after(400), [&] {
             auto* dialog = window->findChild<QObject*>(QStringLiteral("settingsDialog"));
             r.settings = dialog && dialog->property("opened").toBool() &&
                          !app_settings.system().value("decode").toString().isEmpty();
             app_settings.setPreviewQuality(QStringLiteral("half"));
             r.settings = r.settings && preview->scale() == 0.5;
             app_settings.setDecodePath(QStringLiteral("software"));
             const QString folder = smoke_recordings.filePath(QStringLiteral("elsewhere"));
             QDir().mkpath(folder);
             app_settings.setRecordingsFolder(folder);
             r.settings = r.settings && session.recordingsFolder() == folder && session.recordings().isEmpty();
             const AppSettings reread(smoke_recordings.filePath(QStringLiteral("settings.ini")));
             r.settings = r.settings && reread.previewQuality() == QStringLiteral("half") &&
                          reread.decodePath() == QStringLiteral("software") && reread.recordingsFolder() == folder &&
                          reread.startupDecodePath() == QStringLiteral("software");
             write_text(smoke_recordings.filePath(QStringLiteral("broken.ini")),
                        "[preview]\nquality=huge\ndecode=gpu\n[recordings]\nfolder=relative/path\n");
             const AppSettings broken(smoke_recordings.filePath(QStringLiteral("broken.ini")));
             r.settings = r.settings && broken.previewQuality() == QStringLiteral("auto") &&
                          broken.decodePath() == QStringLiteral("auto") && broken.recordingsFolder().isEmpty();
         }},
        {"settings shown", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_SETTINGS_SCREENSHOT");
             const auto notes = items_named("restartNote");
             r.settings = r.settings && notes.size() == 1;
             app_settings.resetAll();
             r.settings = r.settings && preview->scale() == 1.0 && session.recordings().size() == 1 &&
                          app_settings.decodePath() == QStringLiteral("auto");
             const AppSettings reread(smoke_recordings.filePath(QStringLiteral("settings.ini")));
             r.settings = r.settings && reread.previewQuality() == QStringLiteral("auto") && reread.recordingsFolder().isEmpty();
             if (!r.settings) std::printf("GUI smoke: settings failed (%s)\n", qPrintable(app_settings.system().value("decode").toString()));
             // Cache limit persists and the Clear cache button empties both stores (ADR-0009).
             app_settings.setCacheLimit(QStringLiteral("1024"));
             r.settings = r.settings && AppSettings(smoke_recordings.filePath(QStringLiteral("settings.ini"))).cacheLimit() ==
                                            QStringLiteral("1024");
             saved.cache_before = session.cacheBytes();
             session.clearCache(); // the button sits below the fold of the dialog, out of a click's reach
         }},
        {"cache cleared", [&] { return session.cacheBytes() == 0 || run->elapsed() > 4000; }, [&] {
             const bool cleared = saved.cache_before > 0 && session.cacheBytes() == 0;
             if (!cleared) std::printf("GUI smoke: clear cache failed (%llu bytes before, %llu after)\n",
                                       static_cast<unsigned long long>(saved.cache_before),
                                       static_cast<unsigned long long>(session.cacheBytes()));
             r.settings = r.settings && cleared;
             app_settings.resetAll();
             r.settings = r.settings && app_settings.cacheLimit() == QStringLiteral("512");
             actions.trigger(QStringLiteral("settings"));
         }},
        // Opening by origin and single instance (ui-design §3.1): a second launch's files reach
        // the running one (a private socket here), and several videos land in the given order.
        {"second launch", after(100), [&] {
             const QString socket = smoke_recordings.filePath(QStringLiteral("instance.sock"));
             auto* running = new SingleInstance(socket, &application);
             QObject::connect(running, &SingleInstance::received, &application, [&](const QStringList& paths) { saved.forwarded = paths; });
             const QStringList videos{QFileInfo(QStringLiteral("tests/fixtures/generated/hevc_10bit.mp4")).absoluteFilePath(),
                                     QFileInfo(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4")).absoluteFilePath()};
             r.launch = running->listen() && SingleInstance(socket).forward(videos) &&
                        SingleInstance::parse("relative.mp4\n/abs/one.mp4\n") == QStringList{QStringLiteral("/abs/one.mp4")};
             saved.launched = videos;
         }},
        {"forwarded", [&] { return saved.forwarded == saved.launched || run->elapsed() > 3000; }, [&] {
             r.launch = r.launch && saved.forwarded == saved.launched;
             session.openFiles(saved.forwarded);
         }},
        {"opened in order", [&] { return clip_count() == 2 || session.failed() || run->elapsed() > 6000; }, [&] {
             const QVariantList clips = session.clips();
             r.launch = r.launch && clips.size() == 2 && session.editing() &&
                        clips[0].toMap().value("name") == QStringLiteral("hevc_10bit.mp4") &&
                        clips[1].toMap().value("name") == QStringLiteral("h264_30fps_aac.mp4");
             if (!r.launch) std::printf("GUI smoke: launch by files failed (%lld clips)\n", static_cast<long long>(clips.size()));
         }},
        // Live theme and font (omarchy-integration.md §3): a theme switch the way theme-set does
        // it (delete, move, then theme.name), a malformed palette that must change nothing,
        // and a fontconfig change.
        {"theme switch", after(100), [&] {
             const QString root = smoke_recordings.path();
             QDir(root + QStringLiteral("/state/theme")).removeRecursively();
             QDir(root).mkpath(QStringLiteral("state/next-theme"));
             write_text(root + QStringLiteral("/state/next-theme/colors"), "background\t#224466\nforeground\tnot-a-colour\n");
             QDir(root).rename(QStringLiteral("state/next-theme"), QStringLiteral("state/theme"));
             write_text(root + QStringLiteral("/state/theme.name"), "smoke two\n");
         }},
        {"theme switched", [&] { return window->color() == QColor(0x22, 0x44, 0x66) || run->elapsed() > 3000; }, [&] {
             r.theme = window->color() == QColor(0x22, 0x44, 0x66) &&
                       QColor(window->property("fg").value<QColor>()) == QColor(QStringLiteral("#e2e5ee"));
             if (!r.theme) std::printf("GUI smoke: theme switch not applied (%s)\n", qPrintable(window->color().name()));
             const QString root = smoke_recordings.path();
             write_text(root + QStringLiteral("/state/theme/colors"), "garbage\n");
             write_text(root + QStringLiteral("/state/theme.name"), "broken\n");
             saved.font = QGuiApplication::font().family();
             const QStringList families = QFontDatabase::families();
             const auto other = std::ranges::find_if(families, [&](const QString& f) { return f != saved.font; });
             saved.next_font = other == families.end() ? saved.font : *other;
             write_text(root + QStringLiteral("/fontconfig/fonts.conf"), saved.next_font.toUtf8());
         }},
        {"font switched", [&] { return QGuiApplication::font().family() == saved.next_font || run->elapsed() > 3000; }, [&] {
             // A plain Text item, which does not inherit fonts the way Controls do.
             QQuickItem* text = nullptr;
             std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
                 if (text == nullptr && item->isVisible() && item->inherits("QQuickText")) text = item;
                 for (QQuickItem* child : item->childItems()) walk(child);
             };
             walk(window->contentItem());
             const bool font = QGuiApplication::font().family() == saved.next_font && text &&
                               text->property("font").value<QFont>().family() == saved.next_font;
             if (!font) std::printf("GUI smoke: font change not applied (%s)\n",
                                    text ? qPrintable(text->property("font").value<QFont>().family()) : "no text");
             r.theme = r.theme && font && window->color() == QColor(0x22, 0x44, 0x66); // the malformed palette kept the last good one
         }},
        // Autosave and recovery (CLAUDE.md §14): an unsaved edit is autosaved beside, never over,
        // the project; a copy left as if by a crash is offered and restores as unsaved changes
        // to the same project.
        {"autosave project", after(100), [&] { session.saveProject(QUrl::fromLocalFile(project_file)); }},
        {"autosave", [&] { return (!session.dirty() && session.projectPath() == project_file) || run->elapsed() > 4000; }, [&] {
             session.seek(0.3);
             session.splitAtPlayhead();
             saved.clips_after_edit = clip_count();
             session.autosave();
         }},
        {"autosaved", [&] {
             const QDir dir(smoke_recordings.filePath(QStringLiteral("state-home/omamovie/autosave")));
             return !dir.entryList({QStringLiteral("*.omamovie")}, QDir::Files).isEmpty() || run->elapsed() > 4000;
         }, [&] {
             const QString folder = smoke_recordings.filePath(QStringLiteral("state-home/omamovie/autosave"));
             const QStringList autosaves = QDir(folder).entryList({QStringLiteral("*.omamovie")}, QDir::Files);
             r.autosave = autosaves.size() == 1 && session.recoveredProjects().isEmpty(); // its own is not offered
             if (r.autosave) {
                 const QString own = folder + QLatin1Char('/') + autosaves.front();
                 saved.crashed = folder + QStringLiteral("/crashed-1.omamovie");
                 QFile::copy(own, saved.crashed);
                 QFile::copy(own + QStringLiteral(".origin"), saved.crashed + QStringLiteral(".origin"));
             }
             const QVariantList offered = session.recoveredProjects();
             r.autosave = r.autosave && offered.size() == 1 &&
                          offered.front().toMap().value("origin").toString() == session.projectPath();
             session.restoreRecovered(saved.crashed);
         }},
        // Restoring re-imports the library; saving waits for that (Session refuses to save mid-import).
        {"restored", [&] { return (clip_count() == saved.clips_after_edit && !session.media().isEmpty() && !session.importsPending()) ||
                                  run->elapsed() > 6000; }, [&] {
             r.autosave = r.autosave && session.projectPath() == project_file && session.dirty() &&
                          clip_count() == saved.clips_after_edit && session.recoveredProjects().isEmpty();
             session.autosave(); // the restored work keeps autosaving into the recovered file
             session.saveProject(QUrl());
         }},
        {"saved over autosave", [&] {
             const QDir dir(smoke_recordings.filePath(QStringLiteral("state-home/omamovie/autosave")));
             return (!session.dirty() && dir.entryList({QStringLiteral("*.omamovie*")}, QDir::Files).isEmpty()) ||
                    run->elapsed() > 4000;
         }, [&] {
             // Saving the project removes its autosave: nothing is left to recover.
             const QDir dir(smoke_recordings.filePath(QStringLiteral("state-home/omamovie/autosave")));
             r.autosave = r.autosave && !session.dirty() && dir.entryList({QStringLiteral("*.omamovie*")}, QDir::Files).isEmpty();
             if (!r.autosave) std::printf("GUI smoke: autosave recovery failed (dirty %d, left %s, %s)\n", session.dirty(),
                                          qPrintable(dir.entryList({QStringLiteral("*.omamovie*")}, QDir::Files).join(QLatin1Char(','))),
                                          qPrintable(session.status()));
         }},
        // Export (M7): the sequence becomes an MP4 with picture and sound of its length.
        {"export", after(100), [&] {
             saved.export_seconds = session.duration();
             // An unsaved change, then export and save at once: the save must not wait for the export.
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.setClipOpacity(0.9);
             session.seek(0.1); // a caption, so the export writes movie.srt beside the movie (ADR-0017)
             saved.caption_added = session.addCaption() > 0;
             screenshot(window, "OMA_GUI_SMOKE_CAPTION_SCREENSHOT");
             session.exportMovie(QUrl::fromLocalFile(project_dir.filePath(QStringLiteral("movie.mp4"))));
             saved.export_started = session.exportProgress() >= 0;
             session.saveProject(QUrl());
         }},
        {"saved during export", [&] { return !session.dirty() || session.exportProgress() < 0 || run->elapsed() > 10000; }, [&] {
             saved.saved_while_exporting = !session.dirty() && session.exportProgress() >= 0;
             if (!saved.saved_while_exporting) std::printf("GUI smoke: saving waited for the export (dirty %d, progress %.2f)\n",
                                                           session.dirty(), session.exportProgress());
         }},
        {"exporting", after(400), [&] { screenshot(window, "OMA_GUI_SMOKE_EXPORT_SCREENSHOT"); }},
        {"exported", [&] { return session.exportProgress() < 0 || run->elapsed() > 120000; }, [&] {
             const QString file = project_dir.filePath(QStringLiteral("movie.mp4"));
             const auto info = oma::media::FfmpegFormatBackend{}.inspect_input(file.toStdString());
             const double seconds = info && info->duration ? info->duration->seconds_approx() : 0.0;
             bool ok = saved.export_started && session.exportedFile() == file && info && info->best_video &&
                             info->best_audio && std::abs(seconds - saved.export_seconds) < 0.1 &&
                             !QDir(project_dir.path()).entryList({QStringLiteral("movie.mp4.tmp*")}).size();
             if (!ok) std::printf("GUI smoke: export failed (%s; %.3f s for a %.3f s sequence)\n", qPrintable(session.notice()),
                                  seconds, saved.export_seconds);
             const bool sidecar = saved.caption_added && QFile::exists(project_dir.filePath(QStringLiteral("movie.srt")));
             if (!sidecar) std::printf("GUI smoke: no movie.srt beside the export\n");
             ok = ok && sidecar;
             // The notice names the encoder used (Settings: automatic here).
             const bool named = session.notice().contains(QStringLiteral("VA-API")) || session.notice().contains(QStringLiteral("libx264"));
             if (!named) std::printf("GUI smoke: export notice does not name the encoder (%s)\n", qPrintable(session.notice()));
             r.project = r.project && ok && named && saved.saved_while_exporting;
             if (const QString keep = qEnvironmentVariable("OMA_GUI_SMOKE_EXPORT_COPY"); !keep.isEmpty()) QFile::copy(file, keep);
             session.clearExported();
         }},
        // Relink (M7): a project whose file moved away from where automatic search looks offers
        // Locate…; pointing at the file restores it and marks the project changed.
        {"relink setup", after(100), [&] {
             QDir(relink_dir.path()).mkpath(QStringLiteral("a/b"));
             QFile::copy(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4"), relink_dir.filePath(QStringLiteral("a/b/take.mp4")));
             session.newProject();
             session.open(relink_dir.filePath(QStringLiteral("a/b/take.mp4")));
         }},
        {"relink imported", [&] { return (clip_count() == 1 && !session.importsPending()) || run->elapsed() > 5000; }, [&] {
             session.saveProject(QUrl::fromLocalFile(relink_dir.filePath(QStringLiteral("a/relink.omamovie"))));
         }},
        {"relink saved", [&] { return !session.dirty() || session.failed() || run->elapsed() > 4000; }, [&] {
             // Elsewhere and renamed: neither the project's folder nor the name finds it.
             QDir(relink_dir.path()).mkpath(QStringLiteral("moved/deeper"));
             QFile::rename(relink_dir.filePath(QStringLiteral("a/b/take.mp4")), relink_dir.filePath(QStringLiteral("moved/deeper/renamed.mp4")));
             session.openProject(QUrl::fromLocalFile(relink_dir.filePath(QStringLiteral("a/relink.omamovie"))));
         }},
        {"relink missing", [&] { return (session.media().size() == 1 && !session.importsPending()) || run->elapsed() > 5000; }, [&] {
             const bool missing = session.media().value(0).toMap().value("missing").toBool() && session.failed();
             screenshot(window, "OMA_GUI_SMOKE_RELINK_SCREENSHOT");
             if (!missing) std::printf("GUI smoke: the moved file was not reported missing\n");
             saved.relinked = missing;
             // A sound file cannot stand in for a video: refused, still missing.
             session.relinkMedia(0, QUrl::fromLocalFile(QFileInfo(QStringLiteral("tests/fixtures/generated/tone_44100.wav")).absoluteFilePath()));
         }},
        {"relink refused", [&] { return session.notice().contains(QStringLiteral("cannot stand in")) || run->elapsed() > 5000; }, [&] {
             const bool refused = session.notice().contains(QStringLiteral("cannot stand in")) &&
                                  session.media().value(0).toMap().value("missing").toBool();
             if (!refused) std::printf("GUI smoke: a wrong file was not refused as a stand-in (%s)\n", qPrintable(session.notice()));
             saved.relinked = saved.relinked && refused;
             session.relinkMedia(0, QUrl::fromLocalFile(relink_dir.filePath(QStringLiteral("moved/deeper/renamed.mp4"))));
         }},
        {"relinked", [&] { return !session.media().value(0).toMap().value("missing").toBool() || run->elapsed() > 5000; }, [&] {
             const bool ok = saved.relinked && !session.media().value(0).toMap().value("missing").toBool() && session.dirty() &&
                             !session.failed();
             if (!ok) std::printf("GUI smoke: relink failed (dirty %d, failed %d)\n", session.dirty(), session.failed());
             r.project = r.project && ok;
         }},
        // Source viewer (M6 pilot): mark a range of a library item, then add only that range.
        {"source open", after(100), [&] {
             saved.sequence_at = session.position();
             saved.clips_before_source = clip_count();
             session.selectMedia(0);
             actions.trigger(QStringLiteral("openSource"));
             session.seekSource(0.2); // frame 6 at 30 fps
             actions.trigger(QStringLiteral("markIn"));
             session.stepFrames(9);
             actions.trigger(QStringLiteral("markOut")); // keeps frame 15: [6, 16)
         }},
        {"source marked", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_SOURCE_SCREENSHOT");
             const QVariantMap src = session.source();
             const bool marked = src.value("open").toBool() && std::abs(src.value("in").toDouble() - 0.2) < 1e-9 &&
                                 std::abs(src.value("out").toDouble() - 16.0 / 30.0) < 1e-9 &&
                                 session.position() == saved.sequence_at; // the sequence playhead did not move
             actions.trigger(QStringLiteral("append"));
             const QVariantMap added = session.clips().back().toMap();
             const bool exact = clip_count() == saved.clips_before_source + 1 &&
                                std::abs(added.value("duration").toDouble() - 10.0 / 30.0) < 1e-9;
             session.undo();
             const bool undone = clip_count() == saved.clips_before_source;
             // The library says which range it will use; a sequence seek leaves the source viewer.
             const bool badged = std::abs(session.media().value(0).toMap().value("markIn").toDouble() - 0.2) < 1e-9;
             session.seek(0);
             const bool closed = badged && !session.source().value("open").toBool();
             if (!(marked && exact && undone && closed)) {
                 std::printf("GUI smoke: source viewer failed (marked %d, exact %d %.4f s, undone %d, closed %d)\n", marked,
                             exact, added.value("duration").toDouble(), undone, closed);
             }
             r.project = r.project && marked && exact && undone && closed;
         }},
        {"source sound", after(100), [&] {
             // Sound opens in the source viewer too (on an audio track; the picture stays empty).
             session.open(QFileInfo(QStringLiteral("tests/fixtures/generated/tone_44100.wav")).absoluteFilePath());
         }},
        {"source sound open", [&] { return session.media().size() >= 2 || run->elapsed() > 5000; }, [&] {
             const int last = static_cast<int>(session.media().size()) - 1;
             session.openSource(last);
             const bool opened = session.source().value("open").toBool() && session.source().value("duration").toDouble() > 0.5;
             if (!opened) std::printf("GUI smoke: a sound file did not open in the source viewer (%s)\n", qPrintable(session.notice()));
             r.project = r.project && opened;
             session.closeSource();
         }},
        // Larger text (130 %, ui-ux-evaluation gates) at 1600, 820 and 640 px: the timeline keeps
        // its room and the transport stays inside the window; OMA_GUI_SMOKE_TEXT_<width>_SCREENSHOT
        // name files for a visual check.
        {"large text", after(100), [&] {
             app_settings.setTextSize(QStringLiteral("130"));
             window->resize(1600, 900);
         }},
        // Pointer targets (WCAG 2.5.8): every visible, enabled control and mouse area on the edit
        // screen is at least 24 px each way (slider grooves and handles are parts of their slider).
        {"target sizes", after(300), [&] {
             QStringList small;
             std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
                 if (!item->isVisible() || item->opacity() <= 0) return;
                 const QString cls = QString::fromLatin1(item->metaObject()->className());
                 const bool part = cls.contains(QLatin1String("Handle")) || cls.contains(QLatin1String("Groove"));
                 const bool target = !part && (cls.startsWith(QLatin1String("QQuickMouseArea")) || cls.contains(QLatin1String("Button")) ||
                                               cls.contains(QLatin1String("Choice")) || cls.contains(QLatin1String("Slider")));
                 const QPointF at = item->mapToScene(QPointF(0, 0));
                 const bool shown = at.x() < window->width() && at.y() < window->height() && item->width() > 0 && item->height() > 0;
                 if (target && shown && item->isEnabled() && (item->width() < 24 || item->height() < 24))
                     small << QStringLiteral("%1 '%2' %3x%4").arg(cls, item->objectName()).arg(item->width()).arg(item->height());
                 for (QQuickItem* c : item->childItems()) walk(c);
             };
             walk(window->contentItem());
             if (!small.isEmpty()) std::printf("GUI smoke: targets under 24 px: %s\n", qPrintable(small.join(QStringLiteral("; "))));
             r.layout = r.layout && small.isEmpty();
         }},
        {"large text 1600", after(400), [&] { saved.text_ok = large_text_fits("1600"); window->resize(820, 700); }},
        {"large text 820", after(400), [&] { saved.text_ok = large_text_fits("820") && saved.text_ok; window->resize(640, 620); }},
        {"large text 640", after(400), [&] {
             const bool ok = large_text_fits("640") && saved.text_ok;
             app_settings.setTextSize(QStringLiteral("100"));
             window->resize(1600, 900);
             r.layout = r.layout && ok;
         }},
        // The edit workflow by keyboard only (M10): mark a source range (Shift+O, I, Shift+Right, O,
        // Escape), append it (E), split it (Ctrl+B), delete the part under the playhead (Delete),
        // save (Ctrl+S), undo back (Ctrl+Z) and play (Space). File dialogs are left out.
        {"key workflow", after(150), [&] {
             window->requestActivate();
             session.selectMedia(0);
             r.key_clips = clip_count();
             chord(Qt::Key_O, Qt::ShiftModifier);
         }},
        {"key source marks", after(250), [&] {
             r.key_workflow = session.source().value("open").toBool();
             chord(Qt::Key_I, Qt::NoModifier);
             r.key_in = session.source().value("in").toDouble();
             chord(Qt::Key_Right, Qt::ShiftModifier);
             chord(Qt::Key_O, Qt::NoModifier);
             const double out = session.source().value("out").toDouble();
             // Out keeps the frame under the playhead: 11 frames from in.
             r.key_workflow = r.key_workflow && std::abs(out - r.key_in - (11.0 / 30.0)) < 1e-9;
             if (!r.key_workflow) std::printf("GUI smoke: keyboard source marks failed (in %.4f, out %.4f, open %d)\n",
                                              r.key_in, out, session.source().value("open").toBool());
             chord(Qt::Key_Escape, Qt::NoModifier);
         }},
        {"key append", after(200), [&] {
             const bool left = !session.source().value("open").toBool();
             chord(Qt::Key_E, Qt::NoModifier);
             const bool appended = clip_count() == r.key_clips + 1 &&
                                   std::abs(session.clips().back().toMap().value("duration").toDouble() - 11.0 / 30.0) < 1e-9;
             // Into the appended clip, 5 frames past its start (End would be the sound lanes' end).
             chord(Qt::Key_Home, Qt::NoModifier);
             const double appended_at = session.clips().back().toMap().value("start").toDouble();
             for (int i = 0; i < std::lround(appended_at * 30.0) / 10; ++i) chord(Qt::Key_Right, Qt::ShiftModifier);
             for (int i = 0; i < (std::lround(appended_at * 30.0) % 10) + 5; ++i) chord(Qt::Key_Right, Qt::NoModifier);
             chord(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier); // deselect: Delete takes the clip at the playhead
             chord(Qt::Key_B, Qt::ControlModifier);
             const bool split = clip_count() == r.key_clips + 2;
             chord(Qt::Key_Delete, Qt::NoModifier);
             const bool deleted = clip_count() == r.key_clips + 1;
             chord(Qt::Key_S, Qt::ControlModifier);
             r.key_workflow = r.key_workflow && left && appended && split && deleted;
             if (!r.key_workflow) std::printf("GUI smoke: keyboard edit failed (left source %d, appended %d, split %d, deleted %d, clips %lld from %lld)\n",
                                              left, appended, split, deleted, static_cast<long long>(clip_count()),
                                              static_cast<long long>(r.key_clips));
         }},
        {"key saved", [&] { return !session.dirty() || run->elapsed() > 4000; }, [&] {
             const bool saved_by_key = !session.dirty();
             for (int i = 0; i < 3; ++i) chord(Qt::Key_Z, Qt::ControlModifier);
             const bool undone = clip_count() == r.key_clips;
             saved.key_position = session.position();
             chord(Qt::Key_Space, Qt::NoModifier);
             r.key_workflow = r.key_workflow && saved_by_key && undone;
             if (!r.key_workflow) std::printf("GUI smoke: keyboard save/undo failed (saved %d, undone %d)\n", saved_by_key, undone);
         }},
        {"key played", after(400), [&] {
             const bool played = session.playing() || session.position() != saved.key_position;
             chord(Qt::Key_Space, Qt::NoModifier);
             r.key_workflow = r.key_workflow && played;
             if (!played) std::printf("GUI smoke: keyboard playback did not start\n");
         }},
        {"device lost", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_LIBRARY_SCREENSHOT");
             window->setProperty("libraryOverlay", false);
             screenshot(window, "OMA_GUI_SMOKE_SCREENSHOT");
             // Simulated device loss (M4) during playback (M10): the viewer stops and explains while
             // the sound, the transport, editing and saving go on. Last, because nothing may use the
             // device afterwards.
             session.seek(0);
             session.togglePlay();
         }},
        {"device lost while playing", after(300), [&] {
             saved.lost_playing = session.playing();
             device->mark_lost();
         }},
        {"device lost shown", [&] { return session.notice().contains(QStringLiteral("graphics device")) || run->elapsed() > 3000; }, [&] {
             screenshot(window, "OMA_GUI_SMOKE_DEVICE_LOST_SCREENSHOT");
             r.device_lost = saved.lost_playing && session.notice().contains(QStringLiteral("graphics device")) && !session.failed();
             if (!r.device_lost) std::printf("GUI smoke: device loss not shown (playing before %d, notice '%s', failed %d, status '%s')\n",
                                             saved.lost_playing, qPrintable(session.notice()), session.failed(), qPrintable(session.status()));
             saved.lost_position = session.position();
         }},
        {"playing after loss", after(300), [&] {
             // The audio clock keeps running without the GPU; pausing still works.
             const bool advanced = session.playing() && session.position() > saved.lost_position;
             session.pause();
             r.device_lost = r.device_lost && advanced && !session.playing();
             if (!r.device_lost) std::printf("GUI smoke: transport after device loss failed (advanced %d, playing %d)\n",
                                             advanced, session.playing());
             session.selectClip(session.clips().front().toMap().value("id").toDouble());
             session.setClipOpacity(0.5);
             r.device_lost = r.device_lost && session.dirty();
             session.saveProject(QUrl());
         }},
        {"saved after loss", [&] { return !session.dirty() || run->elapsed() > 4000; }, [&] {
             r.device_lost = r.device_lost && !session.dirty();
             if (!r.device_lost) std::printf("GUI smoke: device loss failed (notice '%s', dirty %d)\n",
                                             qPrintable(session.notice()), session.dirty());
         }},
        {"timeline import", after(100), [&] {
             session.importTimeline(QUrl::fromLocalFile(QFileInfo(QStringLiteral("tests/importers/fixtures/basic.otio")).absoluteFilePath()));
         }},
        {"import report", [&] { return !session.importReport().isEmpty() || run->elapsed() > 5000; }, [&] {
             // basic.otio names a file that is not there: the report opens and lists it.
             auto* report = window->findChild<QObject*>(QStringLiteral("importReport"));
             const bool ok = report && report->property("opened").toBool() &&
                             !session.importReport().filter(QStringLiteral("media file not found")).isEmpty();
             if (!ok) std::printf("GUI smoke: import report not shown (%lld notes)\n", static_cast<long long>(session.importReport().size()));
             r.project = r.project && ok;
             if (report) QMetaObject::invokeMethod(report, "close");
         }},
        {"finish", after(100), [&] {
             const bool edits = r.imported && r.split && r.undone && r.redone && r.rippled && r.inserted &&
                                r.overwritten && r.trimmed && r.played && r.audio && r.volume && r.muted &&
                                r.gpu_viewer && r.shuttle && r.lanes && r.looks && r.transitions && r.dragdrop && r.guard &&
                                r.grading && r.project && r.recordings && r.connected && r.timing && r.palette && r.theme && r.launch && r.settings && r.titles && r.autosave && r.device_lost && r.layout;
             const bool keys = r.space && r.escape && r.stepped && r.tab && r.palette_keys && r.menu_keys && r.key_workflow;
             std::printf("GUI smoke: edits %s (import %d, split %d, undo %d, redo %d, ripple delete %d, "
                         "insert %d, overwrite %d, trim %d, play %d, audio %d, volume %d, mute %d, "
                         "GPU viewer %d, J/K/L %d, audio lanes %d, video looks %d, transitions %d, drag and drop %d, "
                         "discard guard %d, grading %d, save and reopen %d, recordings %d, connect %d, framing/freeze/reverse/opacity %d, palette and remap %d, live theme and font %d, open files and single instance %d, settings %d, titles %d, autosave and recovery %d, device loss %d)\n",
                         edits ? "PASS" : "FAIL", r.imported, r.split, r.undone, r.redone, r.rippled,
                         r.inserted, r.overwritten, r.trimmed, r.played, r.audio, r.volume, r.muted,
                         r.gpu_viewer, r.shuttle, r.lanes, r.looks, r.transitions, r.dragdrop, r.guard, r.grading, r.project, r.recordings, r.connected, r.timing, r.palette, r.theme, r.launch, r.settings, r.titles, r.autosave, r.device_lost);
             if (r.keys_focused) {
                 std::printf("GUI smoke: keyboard %s (Tab %d, Space %d, Escape %d, Right x3 -> %.4fs, palette %d, menus %d, edit workflow %d)\n",
                             keys ? "PASS" : "FAIL", r.tab, r.space, r.escape, r.after_steps, r.palette_keys, r.menu_keys, r.key_workflow);
             } else {
                 std::printf("GUI smoke: keyboard skipped (window not focused throughout)\n");
             }
             std::printf("GUI smoke: classic controls %s\n", r.controls ? "PASS" : "FAIL");
             const bool passed = edits && r.controls && (keys || !r.keys_focused) && !session.failed();
             if (session.failed()) std::printf("GUI smoke: error: %s\n", qPrintable(session.status()));
             std::printf("GUI smoke: %s\n", passed ? "PASS" : "FAIL");
             application.exit(passed ? 0 : 1);
         }},
    });
    return application.exec();
}
