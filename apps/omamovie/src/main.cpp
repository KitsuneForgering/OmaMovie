// OmaMovie editor shell: Qt Quick on OmaMovie's Vulkan device (ADR-0005), the M5 timeline
// through Session, with GPU compositing and an opt-in hardware decode pilot.
//
// QtTest drives the --smoke keyboard check. With Qt 6.11 its headers complete classes such as
// QBitArray after a standard-library SFINAE probe saw them incomplete, which GCC 16 reports
// (-Wsfinae-incomplete); the diagnostic is about Qt's header order, not this file.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsfinae-incomplete"
#endif
#include <QTest>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "preview_item.hpp"
#include "session.hpp"
#include "waveform_item.hpp"

#include "oma/gpu/device.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVulkanInstance>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

QVariantMap currentTheme() {
    QVariantMap colors{
        {"background", "#111318"}, {"dark_background", "#171a20"},
        {"darker_background", "#0b0d11"}, {"lighter_background", "#262a33"},
        {"foreground", "#e2e5ee"}, {"dark_foreground", "#8b91a2"},
        {"accent", "#8ea2ff"}, {"selection_background", "#26314a"},
        {"blue", "#4f8dff"}, {"green", "#4dffa6"}, {"yellow", "#ffd75f"},
        {"red", "#ff6b6b"}, {"mode", "dark"}};
    QProcess process;
    process.start(QStringLiteral("omarchy-theme-color"), {QStringLiteral("--all")});
    if (!process.waitForFinished(1000) || process.exitCode() != 0) return colors;
    for (const QByteArray& line : process.readAllStandardOutput().split('\n')) {
        const auto parts = line.split('\t');
        if (parts.size() != 2) continue;
        const QString key = QString::fromUtf8(parts[0]);
        const QString value = QString::fromUtf8(parts[1]).trimmed();
        if (colors.contains(key) && (value.startsWith('#') || key == "mode"))
            colors.insert(key, value);
    }
    return colors;
}

QFont currentFont() {
    QFont fallback = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QProcess process;
    process.start(QStringLiteral("omarchy-font-current"));
    if (!process.waitForFinished(1000) || process.exitCode() != 0) return fallback;
    const QString family = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return family.isEmpty() ? fallback : QFont(family);
}

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

void screenshot(QQuickWindow* window, const char* variable) {
    const QString path = qEnvironmentVariable(variable);
    if (!path.isEmpty()) window->grabWindow().save(path);
}

} // namespace

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    application.setFont(currentFont());
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
    const bool audit = argc == 4 && std::string_view(argv[1]) == "--m4-audit";
    bool valid_seconds = false;
    const int audit_seconds = audit ? QString::fromLocal8Bit(argv[3]).toInt(&valid_seconds) : 0;
    if (!(argc == 1 || smoke || audit || (argc == 2 && argv[1][0] != '-')) ||
        (audit && (!valid_seconds || audit_seconds < 5 || audit_seconds > 3600))) {
        std::fprintf(stderr, "usage: %s [video-file | --smoke | --m4-audit video-file seconds]\n", argv[0]);
        return 2;
    }
    oma::gpu::DeviceOptions options;
    options.internally_synchronized_queues = false;
    options.instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_wayland_surface",
                                   VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME};
    auto created = oma::gpu::Device::create(options);
    if (!created) {
        std::fprintf(stderr, "%s\n", created.error().summary().c_str());
        return 1;
    }
    auto device = std::move(*created);
    QVulkanInstance instance;
    instance.setVkInstance(device->instance());
    instance.setApiVersion(QVersionNumber(1, 4));
    if (!instance.create()) {
        std::fprintf(stderr, "Qt could not adopt the Vulkan instance\n");
        return 1;
    }
    qmlRegisterType<PreviewItem>("OmaMovie", 1, 0, "PreviewItem");
    qmlRegisterType<WaveformItem>("OmaMovie", 1, 0, "WaveformItem");
    Session session;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("session", &session);
    engine.rootContext()->setContextProperty("colors", currentTheme());
    engine.load(QUrl::fromLocalFile(
        QFileInfo(QStringLiteral("apps/omamovie/qml/Main.qml")).absoluteFilePath()));
    if (engine.rootObjects().isEmpty()) return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* preview = window ? window->findChild<PreviewItem*>("preview") : nullptr;
    if (!window || !preview) {
        std::fprintf(stderr, "OmaMovie QML did not create the preview window\n");
        return 1;
    }
    preview->setDevice(device.get());
    session.setPreview(preview);
    window->setVulkanInstance(&instance);
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        device->physical_device(), device->device(), device->graphics_family(), 0));
    QObject::connect(window, &QQuickWindow::beforeFrameBegin, window,
                     [&] { device->lock_queue(device->graphics_family(), 0); },
                     Qt::DirectConnection);
    QObject::connect(window, &QQuickWindow::afterFrameEnd, window,
                     [&] { device->unlock_queue(device->graphics_family(), 0); },
                     Qt::DirectConnection);
    window->show();
    if (audit) {
        // Internal timing diagnostic: the output remains paced by PipeWire (or NullOutput),
        // but its samples are muted. The video timestamp is the last completed composition,
        // which precedes Qt's actual presentation.
        session.setOutputMuted(true);
        session.open(QString::fromLocal8Bit(argv[2]));
        QElapsedTimer waiting;
        waiting.start();
        QElapsedTimer elapsed;
        QTimer timer;
        std::vector<double> errors;
        double min_signed = std::numeric_limits<double>::infinity();
        double max_signed = -std::numeric_limits<double>::infinity();
        double first_sum = 0.0;
        double last_sum = 0.0;
        std::size_t first_count = 0;
        std::size_t last_count = 0;
        std::int64_t initial_underruns = 0;
        double first_underrun_at = -1.0;
        bool started = false;
        bool settled = false;
        QObject::connect(&timer, &QTimer::timeout, &application, [&] {
            if (session.failed()) {
                std::fprintf(stderr, "M4 audit: %s\n", qPrintable(session.status()));
                application.exit(1);
                return;
            }
            if (!started) {
                if (!session.hasMedia()) {
                    if (waiting.elapsed() > 15000) application.exit(1);
                    return;
                }
                if (session.duration() < static_cast<double>(audit_seconds)) {
                    std::fprintf(stderr, "M4 audit: media is shorter than %d seconds\n", audit_seconds);
                    application.exit(2);
                    return;
                }
                session.seek(0);
                session.togglePlay();
                started = true;
                elapsed.start();
                return;
            }
            const double seconds = static_cast<double>(elapsed.elapsed()) / 1000.0;
            if (seconds >= 2.0 && !settled) {
                initial_underruns = session.audioUnderruns();
                settled = true;
            }
            if (settled && first_underrun_at < 0.0 &&
                session.audioUnderruns() > initial_underruns) first_underrun_at = seconds;
            if (seconds >= 2.0 && seconds < static_cast<double>(audit_seconds) &&
                preview->lastCompositedFrame() >= 0) {
                const double audio_time = static_cast<double>(session.audibleSample()) / 48000.0;
                const double video_time = static_cast<double>(preview->lastCompositedFrame()) /
                                          session.frameRate();
                const double signed_error = audio_time - video_time;
                min_signed = std::min(min_signed, signed_error);
                max_signed = std::max(max_signed, signed_error);
                const double error = std::abs(signed_error);
                errors.push_back(error);
                if (seconds < 62.0) { first_sum += signed_error; ++first_count; }
                if (seconds >= static_cast<double>(audit_seconds - 60)) {
                    last_sum += signed_error;
                    ++last_count;
                }
            }
            if (seconds < static_cast<double>(audit_seconds)) return;
            if (errors.empty()) {
                std::fprintf(stderr, "M4 audit: no synchronized frames\n");
                application.exit(1);
                return;
            }
            std::ranges::sort(errors);
            const double p99 = errors[errors.size() * 99 / 100];
            const double maximum = errors.back();
            const double drift = first_count > 0 && last_count > 0
                                     ? std::abs(first_sum / static_cast<double>(first_count) -
                                                last_sum / static_cast<double>(last_count))
                                     : 0.0;
            const auto underruns = session.audioUnderruns() - initial_underruns;
            session.pause();
            const double frame_ms = 1000.0 / session.frameRate();
            const bool passed = p99 * 1000.0 <= frame_ms &&
                                (audit_seconds < 120 || drift * 1000.0 <= frame_ms) &&
                                underruns == 0;
            const std::string dropped = preview->hardwareDecodeAvailable()
                                            ? "unmeasured"
                                            : std::to_string(session.droppedVideoFrames());
            std::printf("M4 audit: %s, %d s, %s audio, %zu samples, %u composites, "
                        "p99 %.2f ms, max %.2f ms, first/last mean delta %.2f ms, "
                        "signed range %.2f..%.2f ms, underruns %lld (first %.2f s), dropped video %s, "
                        "frame interval %.2f ms\n",
                        passed ? "PASS" : "FAIL", audit_seconds,
                        session.audioOnDevice() ? "PipeWire" : "null", errors.size(),
                        preview->presentedFrames(), p99 * 1000.0, maximum * 1000.0,
                        drift * 1000.0, min_signed * 1000.0, max_signed * 1000.0,
                        static_cast<long long>(underruns), first_underrun_at,
                        dropped.c_str(), frame_ms);
            application.exit(passed ? 0 : 1);
        });
        timer.start(10);
        return application.exec();
    }
    if (!smoke) {
        if (argc == 2) session.open(QString::fromLocal8Bit(argv[1]));
        return application.exec();
    }

    window->resize(1600, 900);
    session.setSilent(true);
    struct Results {
        bool imported = false, split = false, undone = false, redone = false, rippled = false;
        bool inserted = false, overwritten = false, trimmed = false, audio = false;
        bool volume = false, muted = false, gpu_viewer = false, shuttle = false;
        bool played = false, space = false, escape = false, stepped = false, keys_focused = true;
        bool controls = false, lanes = false, looks = false, transitions = false, dragdrop = false;
        bool tab = false, guard = false, grading = false, project = false;
        double after_steps = 0;
    } r;
    const auto clip_count = [&] { return session.clips().size(); };
    // Save and reopen (UX-04): what the session held before saving, compared after opening.
    QTemporaryDir project_dir;
    const QString project_file = project_dir.filePath(QStringLiteral("smoke vlog.omamovie"));
    struct Saved {
        qsizetype media = 0, clips = 0, lanes = 0;
        double duration = 0;
        QVariantMap first;
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
    const auto press = [&](Qt::Key key) {
        r.keys_focused = r.keys_focused && QGuiApplication::focusWindow() == window;
        QTest::keyClick(window, key);
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
    QTemporaryDir scratch; // the LUT file the grading step loads
    std::unique_ptr<SmokeRun> run;
    const auto after = [&](qint64 ms) { return [&run, ms] { return run->elapsed() >= ms; }; };
    run = std::make_unique<SmokeRun>(application, std::vector<SmokeRun::Step>{
        {"start", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_PROJECTS_SCREENSHOT");
             session.newProject();
             session.open(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4"));
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
             session.setClipFilter(2, 0.8); // sepia
             session.setClipFraming(1, 0.1, 0, 0.1, 0);
             session.setClipSharpness(-0.3);
             const QVariantMap info = session.info();
             r.looks = info.value("exposure").toDouble() == 0.5 && info.value("filterKind").toInt() == 2 &&
                       info.value("fit").toInt() == 1 && info.value("cropLeft").toDouble() == 0.1 &&
                       info.value("sharpness").toDouble() == -0.3 &&
                       session.undoText() == QStringLiteral("Video Adjustments");
             window->setProperty("drawer", QStringLiteral("effects"));
             session.requestFilterPreviews();
         }},
        {"filter previews", [&] { return session.filterPreviews().size() == 7; }, [&] {
             r.looks = r.looks && std::ranges::all_of(session.filterPreviews(), [](const QVariant& v) {
                 return !v.toString().isEmpty();
             });
         }},
        {"effects drawer", after(400), [&] {
             screenshot(window, "OMA_GUI_SMOKE_EFFECTS_SCREENSHOT");
             window->setProperty("drawer", QString());
             for (int i = 0; i < 4; ++i) session.undo();
             r.looks = r.looks && !session.info().value("colorAdjusted").toBool();
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
             window->setProperty("drawer", QString());
             window->setProperty("cropMore", false);
             for (int i = 0; i < 3; ++i) session.undo();
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
                 drag_mouse(center_of(clips[0]), last_end);
                 ok = session.clips().at(1).toMap().value("id").toDouble() == a && session.undoText() == QStringLiteral("Move");
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
        {"context menu", after(400), [&] {
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
             session.seek(0.5);
             session.togglePlay();
             const bool stop_clicked = click_button("transportStop");
             const bool stopped = stop_clicked && !session.playing() && session.position() == 0.0;
             const bool end_clicked = click_button("transportEnd");
             const bool at_end = end_clicked && session.position() > 0.0;
             const bool start_clicked = click_button("transportStart");
             r.controls = split && stopped && at_end && start_clicked && session.position() == 0.0;
         }},
        {"discard guard", after(300), [&] {
             // With media in the session, New project asks before discarding anything.
             session.showProjects();
         }},
        {"tab", after(200), [&] {
             // Buttons take keyboard focus: from the top of Projects, Tab lands on Open project.
             window->contentItem()->forceActiveFocus();
             press(Qt::Key_Tab);
             const QQuickItem* focused = window->activeFocusItem();
             r.tab = focused != nullptr && focused->objectName() == QStringLiteral("openProject");
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
                      .duration = session.duration(), .first = session.clips().front().toMap()};
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
             if (!r.project) std::printf("GUI smoke: save and reopen failed (%s)\n", qPrintable(session.status()));
         }},
        {"wide window", after(100), [&] {
             window->resize(1600, 900);
         }},
        {"hide library", after(400), [&] {
             // Wide windows fold the library sidebar away and give the viewer the whole width.
             auto* sidebar = window->findChild<QQuickItem*>(QStringLiteral("librarySidebar"));
             auto* viewer = window->findChild<QQuickItem*>(QStringLiteral("viewerColumn"));
             const bool shown = sidebar != nullptr && sidebar->isVisible() && viewer != nullptr && viewer->x() > 0;
             const bool clicked = click_button("toggleLibrary");
             const bool hidden = clicked && !sidebar->isVisible() && viewer->x() == 0.0 &&
                                 viewer->width() == window->contentItem()->width();
             screenshot(window, "OMA_GUI_SMOKE_WIDE_SCREENSHOT");
             window->setProperty("libraryHidden", false);
             r.project = r.project && shown && hidden;
             if (!(shown && hidden)) std::printf("GUI smoke: hiding the library failed\n");
             window->resize(820, 620); // back to the size of the steps before
         }},
        {"library", after(500), [&] {
             window->setProperty("libraryOverlay", true);
         }},
        {"finish", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_LIBRARY_SCREENSHOT");
             window->setProperty("libraryOverlay", false);
             screenshot(window, "OMA_GUI_SMOKE_SCREENSHOT");
             const bool edits = r.imported && r.split && r.undone && r.redone && r.rippled && r.inserted &&
                                r.overwritten && r.trimmed && r.played && r.audio && r.volume && r.muted &&
                                r.gpu_viewer && r.shuttle && r.lanes && r.looks && r.transitions && r.dragdrop && r.guard &&
                                r.grading && r.project;
             const bool keys = r.space && r.escape && r.stepped && r.tab;
             std::printf("GUI smoke: edits %s (import %d, split %d, undo %d, redo %d, ripple delete %d, "
                         "insert %d, overwrite %d, trim %d, play %d, audio %d, volume %d, mute %d, "
                         "GPU viewer %d, J/K/L %d, audio lanes %d, video looks %d, transitions %d, drag and drop %d, "
                         "discard guard %d, grading %d, save and reopen %d)\n",
                         edits ? "PASS" : "FAIL", r.imported, r.split, r.undone, r.redone, r.rippled,
                         r.inserted, r.overwritten, r.trimmed, r.played, r.audio, r.volume, r.muted,
                         r.gpu_viewer, r.shuttle, r.lanes, r.looks, r.transitions, r.dragdrop, r.guard, r.grading, r.project);
             if (r.keys_focused) {
                 std::printf("GUI smoke: keyboard %s (Tab %d, Space %d, Escape %d, Right x3 -> %.4fs)\n",
                             keys ? "PASS" : "FAIL", r.tab, r.space, r.escape, r.after_steps);
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
