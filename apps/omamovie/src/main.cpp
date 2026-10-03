// OmaMovie editor shell: Qt Quick on OmaMovie's Vulkan device (ADR-0005), the M5 timeline
// through Session, and a software-frame viewer until M4's GPU handoff and audio clock.
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

#include "oma/gpu/device.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
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
            std::printf("M4 audit: %s, %d s, %s audio, %zu samples, %u composites, "
                        "p99 %.2f ms, max %.2f ms, first/last mean delta %.2f ms, "
                        "signed range %.2f..%.2f ms, underruns %lld (first %.2f s), dropped video %lld, "
                        "frame interval %.2f ms\n",
                        passed ? "PASS" : "FAIL", audit_seconds,
                        session.audioOnDevice() ? "PipeWire" : "null", errors.size(),
                        preview->presentedFrames(), p99 * 1000.0, maximum * 1000.0,
                        drift * 1000.0, min_signed * 1000.0, max_signed * 1000.0,
                        static_cast<long long>(underruns), first_underrun_at,
                        static_cast<long long>(session.droppedVideoFrames()), frame_ms);
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
        bool controls = false;
        double after_steps = 0;
    } r;
    const auto clip_count = [&] { return session.clips().size(); };
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
             session.seek(0);
             for (int i = 0; i < 3; ++i) press(Qt::Key_Right);
         }},
        {"half width", after(400), [&] {
             r.after_steps = session.position();
             r.stepped = std::abs(r.after_steps - 3.0 / session.frameRate()) < 1e-9;
             window->resize(820, 620);
         }},
        {"classic controls", after(400), [&] {
             session.seek(0.25);
             const int before = clip_count();
             const bool split_clicked = click_button("editSplit");
             const bool split = split_clicked && clip_count() == before + 1;
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
        {"library", after(500), [&] {
             window->setProperty("libraryOverlay", true);
         }},
        {"finish", after(300), [&] {
             screenshot(window, "OMA_GUI_SMOKE_LIBRARY_SCREENSHOT");
             window->setProperty("libraryOverlay", false);
             screenshot(window, "OMA_GUI_SMOKE_SCREENSHOT");
             const bool edits = r.imported && r.split && r.undone && r.redone && r.rippled && r.inserted &&
                                r.overwritten && r.trimmed && r.played && r.audio && r.volume && r.muted &&
                                r.gpu_viewer && r.shuttle;
             const bool keys = r.space && r.escape && r.stepped;
             std::printf("GUI smoke: edits %s (import %d, split %d, undo %d, redo %d, ripple delete %d, "
                         "insert %d, overwrite %d, trim %d, play %d, audio %d, volume %d, mute %d, "
                         "GPU viewer %d, J/K/L %d)\n",
                         edits ? "PASS" : "FAIL", r.imported, r.split, r.undone, r.redone, r.rippled,
                         r.inserted, r.overwritten, r.trimmed, r.played, r.audio, r.volume, r.muted,
                         r.gpu_viewer, r.shuttle);
             if (r.keys_focused) {
                 std::printf("GUI smoke: keyboard %s (Space %d, Escape %d, Right x3 -> %.4fs)\n",
                             keys ? "PASS" : "FAIL", r.space, r.escape, r.after_steps);
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
