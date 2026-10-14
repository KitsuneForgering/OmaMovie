// OmaMovie editor shell: Qt Quick on OmaMovie's Vulkan device (ADR-0005), the M5 timeline
// through Session, with GPU compositing and an opt-in hardware decode pilot.

#include "action_registry.hpp"
#include "app_settings.hpp"
#include "oma/media/format.hpp"
#include "autosave.hpp"
#include "platform/omarchy/theme.hpp"
#include "preview_item.hpp"
#include "gui_smoke.hpp"
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

// The QML next to the executable: an installed /usr/bin/omamovie reads /usr/share/omamovie/qml,
// a development build (build/<config>/omamovie) the source tree's, so edits need no rebuild.
QString qml_dir() {
    const QDir exe(QCoreApplication::applicationDirPath());
    for (const QString& candidate : {QStringLiteral("../share/omamovie/qml"), QStringLiteral("../../apps/omamovie/qml")}) {
        if (QFileInfo::exists(exe.filePath(candidate + QStringLiteral("/Main.qml")))) {
            return QDir::cleanPath(exe.filePath(candidate));
        }
    }
    return QStringLiteral("apps/omamovie/qml");
}

} // namespace

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    // The Wayland app_id, which a Hyprland window rule matches (omarchy-integration.md §4).
    QGuiApplication::setDesktopFileName(QStringLiteral("omamovie"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    const bool smoke = argc == 2 && std::string_view(argv[1]) == "--smoke";
    const bool audit = argc == 4 && std::string_view(argv[1]) == "--m4-audit";
    const bool seek_audit = argc == 4 && std::string_view(argv[1]) == "--m4-seek-audit";
    const bool timeline_audit = argc == 3 && std::string_view(argv[1]) == "--m6-timeline-audit";
    const bool export_audit = argc == 4 && std::string_view(argv[1]) == "--export-audit";
    bool valid_seconds = false;
    const int audit_seconds = audit || seek_audit ? QString::fromLocal8Bit(argv[3]).toInt(&valid_seconds) : 0;
    // Plain arguments are files to open (ui-design §3.1); the run modes take options.
    QStringList files;
    for (int i = 1; i < argc && argv[i][0] != '-'; ++i) files.append(QString::fromLocal8Bit(argv[i]));
    const bool normal = files.size() == argc - 1;
    if (!(normal || smoke || audit || seek_audit || timeline_audit || export_audit) ||
        (audit && (!valid_seconds || audit_seconds < 5 || audit_seconds > 3600)) ||
        (seek_audit && (!valid_seconds || audit_seconds < 5 || audit_seconds > 1000))) {
        std::fprintf(stderr,
                     "usage: %s [project-or-media-file... | --smoke | --m4-audit video-file seconds |"
                     " --m4-seek-audit video-file seeks | --m6-timeline-audit clips | --export-audit video-file out.mp4]\n",
                     argv[0]);
        return 2;
    }
    // Single instance: a normal launch hands its files to a running OmaMovie and exits before
    // touching the GPU. Test and audit runs always start their own.
    SingleInstance instance_channel(SingleInstance::default_socket());
    if (normal) {
        if (instance_channel.forward(files)) return 0;
        if (!instance_channel.listen()) std::fprintf(stderr, "OmaMovie: single-instance socket unavailable\n");
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
    // The smoke run reads recordings from its own folder (UX-09), never the user's.
    QTemporaryDir smoke_recordings;
    if (smoke) {
        QFile::copy(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4"),
                    smoke_recordings.filePath(QStringLiteral("screenrecording-2026-01-01_00-00-00.mp4")));
        qputenv("OMARCHY_SCREENRECORD_DIR", smoke_recordings.path().toLocal8Bit());
        write_smoke_theme(smoke_recordings.path());
        // The smoke run's disk caches (ADR-0009) start empty and never touch the user's.
        qputenv("XDG_CACHE_HOME", smoke_recordings.filePath(QStringLiteral("cache")).toLocal8Bit());
        qputenv("XDG_STATE_HOME", smoke_recordings.filePath(QStringLiteral("state-home")).toLocal8Bit());
    }
    // The theme and font follow Omarchy live; the smoke run uses its own state and scripts.
    omarchy::Theme theme(
        smoke ? smoke_recordings.filePath(QStringLiteral("state"))
              : QDir::homePath() + QStringLiteral("/.local/state/omarchy/current"),
        smoke ? smoke_recordings.filePath(QStringLiteral("fontconfig"))
              : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/fontconfig"));
    theme.load();
    Session session;
    session.setNotifications(true); // desktop notifications for long jobs (M7); audits opt out
    AppSettings app_settings(smoke ? smoke_recordings.filePath(QStringLiteral("settings.ini"))
                                   : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                                         QStringLiteral("/omamovie/settings.ini"));
    // Controls inherit the application font: the Omarchy font at the chosen text size.
    const auto apply_font = [&] {
        QFont font = theme.font();
        const double scale = app_settings.textScale();
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
        else if (font.pixelSize() > 0) font.setPixelSize(static_cast<int>(std::lround(font.pixelSize() * scale)));
        application.setFont(font);
    };
    apply_font();
    QObject::connect(&app_settings, &AppSettings::changed, &application, apply_font);
    session.setRecentProjectsFile(smoke ? smoke_recordings.filePath(QStringLiteral("recent.ini"))
                                        : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                                              QStringLiteral("/omamovie/recent.ini"));
    QObject::connect(&instance_channel, &SingleInstance::received, &session, &Session::openRequested);
    // Shortcut overrides live in the user's config; the smoke run never reads or writes them.
    ActionRegistry actions(smoke ? smoke_recordings.filePath(QStringLiteral("shortcuts.ini"))
                                 : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                                       QStringLiteral("/omamovie/shortcuts.ini"));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("session", &session);
    engine.rootContext()->setContextProperty("actionRegistry", &actions);
    engine.rootContext()->setContextProperty("appSettings", &app_settings);
    engine.rootContext()->setContextProperty("colors", theme.palette());
    // Replacing the context property re-evaluates every binding that reads a colour.
    QObject::connect(&theme, &omarchy::Theme::paletteChanged, &engine,
                     [&] { engine.rootContext()->setContextProperty("colors", theme.palette()); });
    engine.rootContext()->setContextProperty("uiFont", theme.font().family());
    QObject::connect(&theme, &omarchy::Theme::fontChanged, &engine, [&] {
        apply_font();
        engine.rootContext()->setContextProperty("uiFont", theme.font().family());
    });
    engine.load(QUrl::fromLocalFile(qml_dir() + QStringLiteral("/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* preview = window ? window->findChild<PreviewItem*>("preview") : nullptr;
    if (!window || !preview) {
        std::fprintf(stderr, "OmaMovie QML did not create the preview window\n");
        return 1;
    }
    // Opt-in presentation trace for correlating a compositor recording with Wayland resizes.
    // Do not call grabWindow() here: it adds a readback and has disturbed Vulkan validation.
    if (qEnvironmentVariableIsSet("OMA_PRESENT_TRACE")) {
        const auto start = std::chrono::steady_clock::now();
        auto trace_lock = std::make_shared<std::mutex>();
        const auto trace = [start, trace_lock](const char* event, int value = -1) {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
            const std::scoped_lock lock(*trace_lock);
            std::fprintf(stderr, "present-trace us=%lld event=%s value=%d\n",
                         static_cast<long long>(us), event, value);
        };
        QObject::connect(window, &QWindow::widthChanged, window,
                         [trace](int width) { trace("width", width); });
        QObject::connect(window, &QWindow::heightChanged, window,
                         [trace](int height) { trace("height", height); });
        QObject::connect(window, &QWindow::visibilityChanged, window,
                         [trace](QWindow::Visibility state) { trace("visibility", static_cast<int>(state)); });
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
                         [trace] { trace("frame-swapped"); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window,
                         [trace] { trace("scene-graph-initialized"); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window,
                         [trace] { trace("scene-graph-invalidated"); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::sceneGraphError, window,
                         [trace](QQuickWindow::SceneGraphError error, const QString& message) {
                             trace("scene-graph-error", static_cast<int>(error));
                             std::fprintf(stderr, "present-trace detail=%s\n", message.toUtf8().constData());
                         });
    }
    preview->setDevice(device.get());
    // Settings (M6): the decode path is fixed for the run; quality and recordings apply live.
    const QString decode = app_settings.startupDecodePath();
    preview->setDecodePreference(decode == QLatin1String("hardware")   ? PreviewItem::DecodePreference::Hardware
                                 : decode == QLatin1String("software") ? PreviewItem::DecodePreference::Software
                                                                       : PreviewItem::DecodePreference::Auto);
    session.setPreview(preview);
    const auto apply_settings = [&] {
        session.setPreviewQuality(app_settings.previewQuality());
        session.setRecordingsFolderOverride(app_settings.recordingsFolder());
        session.setCacheBudget(app_settings.cacheLimit().toULongLong() << 20);
        session.setExportEncoder(app_settings.exportEncoder());
    };
    apply_settings();
    QObject::connect(&app_settings, &AppSettings::changed, &session, apply_settings);
    QObject::connect(&session, &Session::cacheCleared, &app_settings, &AppSettings::refreshSystem);
    QObject::connect(&app_settings, &AppSettings::systemRequested, &session, [&] {
        const oma::gpu::DeviceInfo& gpu = device->info();
        QStringList overrides;
        for (const char* name : {"OMA_PREVIEW_HARDWARE", "OMA_PREVIEW_SCALE", "OMARCHY_SCREENRECORD_DIR"}) {
            if (qEnvironmentVariableIsSet(name)) overrides.append(QStringLiteral("%1=%2").arg(QLatin1String(name), qEnvironmentVariable(name)));
        }
        app_settings.setSystem(QVariantMap{
            {"gpu", QString::fromStdString(gpu.name)},
            {"driver", QString::fromStdString(gpu.driver_name + " " + gpu.driver_info).trimmed()},
            {"decode", preview->decodeStatus()},
            {"audio", session.audioOnDevice() ? QStringLiteral("PipeWire, the system's default output")
                                              : QStringLiteral("PipeWire, opened when playback starts (silent if no device)")},
            {"font", theme.font().family()},
            {"cache", QStringLiteral("%1 (%2 MB)").arg(session.cacheFolder()).arg(
                          static_cast<double>(session.cacheBytes()) / (1024.0 * 1024.0), 0, 'f', 1)},
            {"overrides", overrides.join(QStringLiteral(", "))}});
    });
    window->setVulkanInstance(&instance);
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        device->physical_device(), device->device(), device->graphics_family(), 0));
    // ADR-0005: Qt creates, resizes and destroys its swapchain and waits for the device to
    // idle between frames, so other threads are admitted to the device only within a frame.
    // Qt's own queue operations in a frame are endFrame's submit and present, so it holds the
    // graphics queue from afterRendering on; before that, admitted decoders can submit (on a
    // single-queue GPU such as the Iris Xe they share this queue).
    // Qt does not always pair afterRendering with afterFrameEnd (a window capture renders
    // outside the normal frame), so the hold is a flag rather than a count: taken at most once,
    // released at the frame end or, at the latest, when the next frame begins. Render thread only.
    device->close_admission();
    bool queue_held = false;
    const auto release_queue = [&] {
        if (std::exchange(queue_held, false)) device->unlock_queue(device->graphics_family(), 0);
    };
    QObject::connect(window, &QQuickWindow::beforeFrameBegin, window,
                     [&] {
                         release_queue();
                         device->open_admission();
                     },
                     Qt::DirectConnection);
    QObject::connect(window, &QQuickWindow::afterRendering, window,
                     [&] {
                         if (!std::exchange(queue_held, true)) {
                             device->lock_queue(device->graphics_family(), 0);
                         }
                     },
                     Qt::DirectConnection);
    QObject::connect(window, &QQuickWindow::afterFrameEnd, window,
                     [&] {
                         // Release the queue first: admitted work may be waiting for it.
                         release_queue();
                         device->close_admission();
                     },
                     Qt::DirectConnection);
    window->show();
    // M4 audit: Qt's frame phases on the render thread (begin -> sync -> render -> end).
    struct QtPhases {
        std::mutex m;
        std::vector<double> sync, render, end, frame;
        std::chrono::steady_clock::time_point begin, synced, rendered;
    };
    auto phases = std::make_shared<QtPhases>();
    if (audit) {
        using Clock = std::chrono::steady_clock;
        const auto ms = [](Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };
        QObject::connect(window, &QQuickWindow::beforeFrameBegin, window,
                         [phases] { phases->begin = Clock::now(); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::afterSynchronizing, window,
                         [phases] { phases->synced = Clock::now(); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::afterRendering, window,
                         [phases] { phases->rendered = Clock::now(); }, Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::afterFrameEnd, window,
                         [phases, ms] {
                             const auto now = Clock::now();
                             const std::scoped_lock lock(phases->m);
                             if (phases->frame.size() > 65536) return;
                             phases->sync.push_back(ms(phases->begin, phases->synced));
                             phases->render.push_back(ms(phases->synced, phases->rendered));
                             phases->end.push_back(ms(phases->rendered, now));
                             phases->frame.push_back(ms(phases->begin, now));
                         },
                         Qt::DirectConnection);
    }
    if (export_audit) {
        // M7: exports one file placed on the storyline and reports where the time went.
        const QString out = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
        session.setSilent(true);
        session.setNotifications(false);
        session.openFiles({QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath()});
        QElapsedTimer waited;
        waited.start();
        while ((session.clips().isEmpty() || session.importsPending()) && !session.failed() && waited.elapsed() < 20000) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 20);
        }
        // Retried until the library has loaded every clip's media (exportMovie refuses before).
        while (session.exportProgress() < 0 && session.exportedFile() != out && waited.elapsed() < 30000) {
            session.exportMovie(QUrl::fromLocalFile(out));
            if (session.exportProgress() < 0) QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
        }
        while (session.exportProgress() >= 0) QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 20);
        const ExportStats& st = session.lastExportStats();
        const double n = static_cast<double>(std::max<std::int64_t>(1, st.frames));
        std::printf("export audit: %lld frames in %.0f ms (%.1f fps) -> %s\n  encoder: %s\n"
                    "  per frame: build %.2f ms, render+readback %.2f ms, convert %.2f ms, encode %.2f ms, audio %.2f ms\n"
                    "  result: %s\n",
                    static_cast<long long>(st.frames), st.total, n * 1000.0 / std::max(1.0, st.total), qPrintable(out), st.encoder.c_str(),
                    st.build / n, st.render / n, st.convert / n, st.encode / n, st.audio / n,
                    qPrintable(session.exportedFile() == out ? QStringLiteral("written") : session.notice()));
        return session.exportedFile() == out ? 0 : 1;
    }
    if (timeline_audit) {
        // M6 long-form gate (Docs/Research/long-form-editing.md): a storyline of N short clips
        // built with the timeline's own commands, saved with libs/project and opened in the
        // editor; then routine edits, each timed from the call to the end of the next Qt frame
        // that started after it (model rebuild, QML bindings and rendering included).
        const int clips = QString::fromLocal8Bit(argv[2]).toInt();
        QTemporaryDir dir;
        const QString base = dir.filePath(QStringLiteral("base.omamovie"));
        const QString longp = dir.filePath(QStringLiteral("long.omamovie"));
        std::atomic<int> frames{0}; // ended
        std::atomic<int> begun{0};
        QObject::connect(window, &QQuickWindow::beforeFrameBegin, window, [&begun] { begun.fetch_add(1); },
                         Qt::DirectConnection);
        QObject::connect(window, &QQuickWindow::afterFrameEnd, window, [&frames] { frames.fetch_add(1); },
                         Qt::DirectConnection);
        using Clock = std::chrono::steady_clock;
        const auto ms_since = [](Clock::time_point t) {
            return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        };
        // Runs `action`, then waits (processing events) until a frame that began after it ended.
        const auto timed = [&](const std::function<void()>& action) {
            const auto t0 = Clock::now();
            action();
            window->update();
            // The next frame to begin is the first that can show the change; wait for its end.
            const int target = begun.load() + 1;
            while (frames.load() < target && ms_since(t0) < 5000) {
                QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 5);
            }
            return ms_since(t0);
        };
        const auto wait_for = [&](const std::function<bool()>& done, int ms) {
            const auto t0 = Clock::now();
            while (!done() && ms_since(t0) < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            return done();
        };
        QTimer::singleShot(300, &application, [&] {
            session.newProject();
            session.open(QStringLiteral("tests/fixtures/generated/h264_30fps_aac.mp4"));
            if (!wait_for([&] { return session.clips().size() == 1; }, 15000)) { application.exit(1); return; }
            session.saveProject(QUrl::fromLocalFile(base));
            if (!wait_for([&] { return !session.dirty(); }, 15000)) { application.exit(1); return; }
            // N clips of 5 frames each, built and saved without the UI.
            auto doc = oma::project::load(base.toStdString());
            if (!doc) { std::fprintf(stderr, "M6 audit: %s\n", doc.error().summary().c_str()); application.exit(1); return; }
            oma::timeline::Editor ed(std::move(*doc->timeline));
            const oma::timeline::Clip first = ed.timeline().find_track(doc->storyline)->clips.front();
            std::vector<std::unique_ptr<oma::timeline::Command>> steps;
            for (int i = 1; i < clips; ++i) {
                steps.push_back(oma::timeline::edit::append(
                    doc->storyline, ed.new_clip_id(),
                    {.media = first.media, .source_in = first.source_in,
                     .duration = ed.timeline().at(first.duration.value() / 6), .time_map = {}, .video = {}, .audio = {}}));
            }
            const auto build0 = Clock::now();
            if (auto r = ed.execute(oma::timeline::edit::transaction("Fill", std::move(steps))); !r) {
                std::fprintf(stderr, "M6 audit: %s\n", r.error().summary().c_str());
                application.exit(1);
                return;
            }
            const double build_ms = ms_since(build0);
            doc->timeline = ed.timeline();
            if (auto r = oma::project::save(*doc, longp.toStdString()); !r) { application.exit(1); return; }
            const double open_ms = timed([&] {
                session.openProject(QUrl::fromLocalFile(longp));
                wait_for([&] { return session.clips().size() == clips; }, 60000);
            });
            if (session.clips().size() != clips) { std::fprintf(stderr, "M6 audit: open failed\n"); application.exit(1); return; }
            // OMA_AUDIT_ZOOM=pixels-per-second: a working zoom with real clip delegates in view
            // instead of the whole sequence fitted (the default).
            if (const QByteArray zoom = qgetenv("OMA_AUDIT_ZOOM"); !zoom.isEmpty()) {
                window->setProperty("fitTimeline", false);
                window->setProperty("pixelsPerSecond", zoom.toDouble());
                wait_for([] { return false; }, 300);
            }
            std::vector<double> seek, split, ripple, undo_ms, select;
            std::uint64_t seed = 12345;
            const auto next = [&] {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                return static_cast<double>(seed >> 11) / static_cast<double>(1ULL << 53);
            };
            for (int i = 0; i < 20; ++i) {
                const double at = next() * (session.duration() - 0.1);
                seek.push_back(timed([&] { session.seek(at); }));
                split.push_back(timed([&] { session.splitAtPlayhead(); }));
                undo_ms.push_back(timed([&] { session.undo(); }));
                const QVariantList list = session.clips();
                const double id = list[static_cast<qsizetype>(next() * static_cast<double>(list.size() - 1))].toMap().value("id").toDouble();
                select.push_back(timed([&] { session.selectClip(id); }));
                ripple.push_back(timed([&] { session.deleteSelected(true); }));
                undo_ms.push_back(timed([&] { session.undo(); }));
            }
            // A continuous tail drag on a middle clip: four pointer events per frame of trim, as
            // at a working zoom, so most events repeat the previous snapped frame.
            std::vector<double> drag;
            int scopes = 0;
            const auto counter = QObject::connect(&session, &Session::editScopeChanged, &application,
                                                  [&scopes] { ++scopes; });
            const QVariantList list = session.clips();
            const double dragged = list[list.size() / 2].toMap().value("id").toDouble();
            constexpr int kDragEvents = 100;
            for (int i = 1; i <= kDragEvents; ++i)
                drag.push_back(timed([&] { session.previewTrim(dragged, false, i / 4); }));
            QObject::disconnect(counter);
            std::vector<double> release{timed([&] { session.trimClip(dragged, false, kDragEvents / 4); })};
            release.push_back(timed([&] { session.undo(); }));
            const auto summary = [](std::vector<double> v) {
                std::ranges::sort(v);
                return std::array<double, 3>{v[v.size() / 2], v[v.size() * 95 / 100], v.back()};
            };
            const auto line = [&](const char* name, const std::vector<double>& v) {
                const auto x = summary(v);
                std::printf("  %-12s p50 %8.1f  p95 %8.1f  max %8.1f ms\n", name, x[0], x[1], x[2]);
            };
            std::ifstream status("/proc/self/status");
            std::string rss;
            std::string peak;
            for (std::string l; std::getline(status, l);) {
                if (l.rfind("VmRSS", 0) == 0) rss = l.substr(6);
                if (l.rfind("VmHWM", 0) == 0) peak = l.substr(6);
            }
            std::printf("M6 timeline audit: %d clips, model build %.1f ms, open %.1f ms (save, load, reimport, UI), RSS%s, peak%s\n",
                        clips, build_ms, open_ms, rss.c_str(), peak.c_str());
            line("seek", seek);
            line("select", select);
            line("split", split);
            line("ripple del", ripple);
            line("undo", undo_ms);
            std::printf("  trim drag: %d pointer events, %d scope updates\n", kDragEvents, scopes);
            line("drag event", drag);
            line("trim, undo", release);
            application.exit(0);
        });
        return application.exec();
    }
    if (seek_audit) {
        // Seeks during playback every 500 ms to pseudo-random positions (fixed seed) and records
        // how long each restart held the UI thread and how long until a composited frame from
        // the new position (composition, not Qt's presentation).
        session.setOutputMuted(true);
        session.open(QString::fromLocal8Bit(argv[2]));
        QElapsedTimer waiting;
        waiting.start();
        QElapsedTimer since_seek;
        QTimer timer;
        std::vector<double> blocked;
        std::vector<double> first_frame;
        std::uint64_t seed = 0x9E3779B97F4A7C15ULL;
        std::int64_t target_frame = -1;
        bool started = false;
        bool waiting_frame = false;
        QObject::connect(&timer, &QTimer::timeout, &application, [&] {
            if (session.failed()) {
                std::fprintf(stderr, "M4 seek audit: %s\n", qPrintable(session.status()));
                application.exit(1);
                return;
            }
            if (!started) {
                if (!session.hasMedia()) {
                    if (waiting.elapsed() > 15000) application.exit(1);
                    return;
                }
                if (session.duration() < 10.0) {
                    std::fprintf(stderr, "M4 seek audit: media is shorter than 10 s\n");
                    application.exit(2);
                    return;
                }
                session.togglePlay();
                started = true;
                since_seek.start();
                return;
            }
            if (waiting_frame) {
                const std::int64_t shown = preview->lastCompositedFrame();
                if (shown >= target_frame && shown <= target_frame + 30) {
                    first_frame.push_back(static_cast<double>(since_seek.nsecsElapsed()) / 1e6);
                    waiting_frame = false;
                }
            }
            if (since_seek.elapsed() < 500) return;
            if (waiting_frame) first_frame.push_back(-1.0); // no frame within 500 ms
            if (std::cmp_greater_equal(blocked.size(), audit_seconds)) {
                session.pause();
                const auto summary = [](std::vector<double> v) {
                    std::ranges::sort(v);
                    return std::array<double, 3>{v[v.size() / 2], v[v.size() * 95 / 100], v.back()};
                };
                const auto missed = std::ranges::count(first_frame, -1.0);
                std::erase(first_frame, -1.0);
                const auto b = summary(blocked);
                const auto f = first_frame.empty() ? std::array<double, 3>{} : summary(first_frame);
                std::printf("M4 seek audit: %zu seeks, %s audio, UI blocked p50 %.2f p95 %.2f max %.2f ms, "
                            "first composited frame p50 %.2f p95 %.2f max %.2f ms, %lld without a frame "
                            "in 500 ms\n",
                            blocked.size(), session.audioOnDevice() ? "PipeWire" : "null", b[0], b[1], b[2],
                            f[0], f[1], f[2], static_cast<long long>(missed));
                application.exit(0);
                return;
            }
            seed = (seed * 6364136223846793005ULL) + 1442695040888963407ULL;
            const double span = session.duration() - 2.0;
            const double target = span * static_cast<double>(seed >> 11) / static_cast<double>(1ULL << 53);
            target_frame = std::llround(target * session.frameRate());
            since_seek.restart();
            session.seek(target);
            blocked.push_back(session.lastRestartMs());
            waiting_frame = true;
        });
        timer.start(2);
        return application.exec();
    }
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
            const std::string dropped = std::to_string(session.droppedVideoFrames());
            auto timings = preview->takeTimings();
            const auto pct = [](std::vector<double>& v, int p) {
                if (v.empty()) return 0.0;
                std::ranges::sort(v);
                return v[std::min(v.size() - 1, v.size() * static_cast<std::size_t>(p) / 100)];
            };
            {
                const std::scoped_lock lock(phases->m);
                std::printf("M4 audit Qt frame: begin->synced p50 %.2f p99 %.2f, synced->rendered p50 %.2f "
                            "p99 %.2f, rendered->end p50 %.2f p99 %.2f, whole p50 %.2f p99 %.2f ms\n",
                            pct(phases->sync, 50), pct(phases->sync, 99), pct(phases->render, 50),
                            pct(phases->render, 99), pct(phases->end, 50), pct(phases->end, 99),
                            pct(phases->frame, 50), pct(phases->frame, 99));
            }
            auto [tick, gap] = session.takeTickTimings();
            std::printf("M4 audit UI: tick p50 %.2f p95 %.2f p99 %.2f ms, gap p50 %.2f p95 %.2f p99 %.2f ms\n",
                        pct(tick, 50), pct(tick, 95), pct(tick, 99), pct(gap, 50), pct(gap, 95), pct(gap, 99));
            std::printf("M4 audit stages: composite p50 %.2f p95 %.2f p99 %.2f ms, interval p50 %.2f p95 %.2f "
                        "p99 %.2f ms\n",
                        pct(timings.composite_ms, 50), pct(timings.composite_ms, 95), pct(timings.composite_ms, 99),
                        pct(timings.interval_ms, 50), pct(timings.interval_ms, 95), pct(timings.interval_ms, 99));
            std::printf("M4 audit: %s, %d s, %s decode, %s audio, %zu samples, %u composites, "
                        "p99 %.2f ms, max %.2f ms, first/last mean delta %.2f ms, "
                        "signed range %.2f..%.2f ms, underruns %lld (first %.2f s), dropped video %s, "
                        "frame interval %.2f ms\n",
                        passed ? "PASS" : "FAIL", audit_seconds,
                        preview->hardwareDecodeAvailable() ? "hardware pilot" : "software",
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
        session.openFiles(files);
        return application.exec();
    }

    return run_gui_smoke({.application = application,
                          .window = window,
                          .preview = preview,
                          .session = session,
                          .device = *device,
                          .actions = actions,
                          .settings = app_settings,
                          .recordings = smoke_recordings});
}
