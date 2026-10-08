#include "session.hpp"

#include "autosave.hpp"
#include "exporter.hpp"
#include "desktop_notify.hpp"

#include "preview_item.hpp"
#include "platform/omarchy/recordings.hpp"

#include "oma/base/log.hpp"
#include "oma/compositor/compositor.hpp"
#include "oma/compositor/geometry.hpp"
#include "oma/compositor/grade.hpp"
#include "oma/compositor/render_graph.hpp"
#include "oma/project/subtitles.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/effects.hpp"
#include "oma/timeline/evaluate.hpp"

#include <QColor>
#include <QGuiApplication>
#include <QBuffer>
#include <QDateTime>
#include <QLocale>
#include <QFile>
#include <QSaveFile>
#include <QStringList>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace tl = oma::timeline;

namespace {

// ADR-0009 defaults: each store keeps at most this much.
constexpr std::uint64_t kCacheBudget = std::uint64_t{512} << 20;

std::filesystem::path cache_dir(const char* kind) {
    const auto root = oma::DiskCache::default_root();
    return root.empty() ? root : root / kind;
}

// The content a cache key stands for (ADR-0009): the file, its size and time, and the
// project fingerprint's partial hash, so an edited or replaced file gets new entries.
std::string media_identity(const QString& path, const oma::project::Fingerprint& fingerprint) {
    std::error_code ec;
    const std::filesystem::path file(path.toStdString());
    const auto time = std::filesystem::last_write_time(file, ec);
    const auto ns = ec ? 0 : std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
    return std::format("{} {} {} {:016x}", file.string(), fingerprint.size, ns, fingerprint.hash);
}

} // namespace

namespace {

// What a clip's time map does, for its badge on the timeline (ADR-0013); empty at normal speed.
QString timingLabel(const tl::TimeMap& map) {
    if (!map.is_constant()) {
        bool reverse = false;
        bool freeze = false;
        bool other = false;
        bool ramp = false;
        for (const tl::TimeSegment& seg : map.segments()) {
            ramp = ramp || seg.kind == tl::TimeSegment::Kind::Ramp;
            reverse = reverse || seg.from.num() < 0 || seg.to.num() < 0;
            freeze = freeze || seg.kind == tl::TimeSegment::Kind::Freeze;
            other = other || (seg.kind == tl::TimeSegment::Kind::Linear && seg.from != oma::Rational::literal(1, 1) &&
                     seg.from != oma::Rational::literal(-1, 1));
        }
        QStringList parts;
        if (reverse) parts << QStringLiteral("Reverse");
        if (freeze) parts << QStringLiteral("Freeze");
        if (ramp) parts << QStringLiteral("Ramp");
        if (other) parts << QStringLiteral("Retimed");
        return parts.join(QStringLiteral(" · "));
    }
    const oma::Rational s = map.speed();
    if (s == oma::Rational::literal(1, 1)) return {};
    return s.den() == 1 ? QStringLiteral("%1×").arg(s.num()) : QStringLiteral("%1/%2×").arg(s.num()).arg(s.den());
}


constexpr int kNoticeMs = 3500;
constexpr std::int64_t kStillSeconds = 4; // default length of a picture on the storyline
const auto kSequenceAudioRate = oma::SampleRate::make(48000).value();

QString message(const oma::Error& error) {
    return QString::fromStdString(error.message());
}

} // namespace

Session::Session(QObject* parent)
    : Session(std::make_unique<oma::media::FfmpegFormatBackend>(), parent) {}

Session::Session(std::unique_ptr<oma::media::MediaImporter> importer, QObject* parent)
    : QObject(parent),
      thumbnail_cache_(cache_dir("thumbnails"), kCacheBudget),
      waveform_cache_(cache_dir("waveforms"), kCacheBudget),
      importer_(importer ? std::move(importer) : std::make_unique<oma::media::FfmpegFormatBackend>()),
      audio_(kSequenceAudioRate) {
    waveforms_.set_cache(&waveform_cache_);
    autosave_timer_.setInterval(kAutosaveSeconds * 1000);
    connect(&autosave_timer_, &QTimer::timeout, this, &Session::autosave);
    autosave_timer_.start();
    // ADR-0009: trim what earlier sessions left over the budget, off the UI thread.
    (void)workers_.submit("cache-evict", [this](oma::JobContext&) {
        for (const oma::DiskCache* cache : {&thumbnail_cache_, &waveform_cache_}) {
            if (auto removed = cache->evict(); removed && *removed > 0) {
                oma::log_info(oma::Category::Cache, "cache trimmed by {} bytes in {}", *removed, cache->dir().string());
            }
        }
        return oma::Result<void>{};
    });
    // Export progress crosses threads through one atomic that this timer reads (CLAUDE.md §13).
    export_timer_.setInterval(200);
    connect(&export_timer_, &QTimer::timeout, this, [this] {
        export_progress_ = export_total_ > 0 ? static_cast<double>(export_done_.load()) / static_cast<double>(export_total_) : 0;
        emit exportChanged();
    });
    notice_timer_.setSingleShot(true);
    connect(&notice_timer_, &QTimer::timeout, this, [this] {
        notice_.clear();
        emit statusChanged();
    });
    tick_.setTimerType(Qt::PreciseTimer);
    connect(&tick_, &QTimer::timeout, this, &Session::onTick);
    // Before any QML binding sees the change (connected first).
    connect(this, &Session::sequenceChanged, this, [this] {
        clips_cache_.reset();
        syncStoryline();
    });
    connect(this, &Session::libraryChanged, this, [this] {
        clips_cache_.reset();
        syncStoryline();
    });
    connect(this, &Session::positionChanged, this, &Session::motionChanged);
    recordings_settle_.setInterval(2000);
    connect(&recordings_settle_, &QTimer::timeout, this, &Session::refreshRecordings);
    connect(&recordings_watcher_, &QFileSystemWatcher::directoryChanged, this, &Session::refreshRecordings);
    refreshRecordings();
    connect(this, &Session::selectionChanged, this, &Session::motionChanged);
    connect(this, &Session::libraryChanged, this, &Session::projectChanged);
    connect(this, &Session::sequenceChanged, this, &Session::projectChanged);
    connect(this, &Session::lutsChanged, this, &Session::projectChanged);
}

Session::~Session() {
    export_job_.cancel(); // stops at the next frame; the temporary output is removed
    export_pool_.shutdown();
    workers_.shutdown();
}

// ------------------------------------------------------------------- screens

void Session::newProject() {
    discardAutosave(); // the user chose to leave this work (callers confirm unsaved changes)
    ++generation_;
    queued_files_.clear();
    pause();
    for (oma::JobHandle& h : imports_) h.cancel();
    imports_.clear();
    library_.clear();
    waveforms_.clear();
    filter_previews_.clear();
    emit filterPreviewsChanged();
    editor_.reset();
    snapshot_.reset();
    source_index_ = -1;
    source_timeline_.reset();
    emit sourceChanged();
    refreshPaths();
    luts_ = std::make_shared<const LutTables>();
    lut_paths_.clear();
    project_path_.clear();
    saved_ = {false, 0, 0, 0, library_revision_};
    emit lutsChanged();
    primary_ = {};
    selected_clip_ = {};
    selected_media_ = -1;
    playhead_ = 0;
    wanted_.reset();
    failed_ = false;
    status_.clear();
    notice_.clear();
    if (preview_ != nullptr) preview_->setFrame(nullptr);
    editing_ = true;
    emit libraryChanged();
    emit sequenceChanged();
    emit selectionChanged();
    emit positionChanged();
    emit statusChanged();
    emit viewChanged();
}

void Session::showProjects() {
    pause();
    editing_ = false;
    emit viewChanged();
}

void Session::continueProject() {
    editing_ = true;
    emit viewChanged();
}

// ------------------------------------------------------------------- import

void Session::importUrl(const QUrl& url) {
    importFile(url.toLocalFile(), !hasMedia());
}

void Session::refreshRecordings() {
    const auto dir = recordings_override_.isEmpty() ? omarchy::recordings_dir()
                                                    : std::filesystem::path(recordings_override_.toStdString());
    recordings_folder_ = QString::fromStdString(dir.string());
    if (!recordings_watcher_.directories().contains(recordings_folder_)) {
        if (!recordings_watcher_.directories().isEmpty()) {
            recordings_watcher_.removePaths(recordings_watcher_.directories());
        }
        recordings_watcher_.addPath(recordings_folder_); // fails quietly if it does not exist
    }
    QVariantList list;
    bool growing = false;
    for (const omarchy::Recording& r : omarchy::list_recordings(dir, 12)) {
        const auto when = QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::clock_cast<std::chrono::system_clock>(r.modified)));
        const QString name = QString::fromStdString(r.path.filename().string());
        // The capture script's own names carry nothing beyond the date shown below.
        const QString title = name.startsWith(QStringLiteral("screenrecording-")) ? QStringLiteral("Screen recording") : name;
        // The script writes a "-preview.png" next to finished captures; no decode needed then.
        auto preview = r.path;
        preview.replace_extension();
        preview += "-preview.png";
        std::error_code ec;
        const QString thumbnail = std::filesystem::exists(preview, ec)
                                      ? QUrl::fromLocalFile(QString::fromStdString(preview.string())).toString()
                                      : QString();
        // Recordings may have no sound (no audio device was selected): say so before editing.
        const QString path = QString::fromStdString(r.path.string());
        const QString key = path + QLatin1Char('|') + when.toString(Qt::ISODateWithMs);
        const auto audio = recording_audio_.constFind(key);
        if (audio == recording_audio_.constEnd() && !r.growing && !recording_probes_.contains(key)) {
            recording_probes_.insert(key);
            (void)workers_.submit("probe-recording", [this, key, file = r.path](oma::JobContext&) {
                auto probed = importer_->inspect_input(file);
                const bool has_audio = probed && probed->best_audio.has_value();
                QMetaObject::invokeMethod(this, [this, key, has_audio] {
                    recording_audio_.insert(key, has_audio);
                    refreshRecordings();
                }, Qt::QueuedConnection);
                return oma::Result<void>{};
            });
        }
        list.push_back(QVariantMap{{QStringLiteral("path"), path},
                                   {QStringLiteral("url"), QUrl::fromLocalFile(path)},
                                   {QStringLiteral("noAudio"), audio != recording_audio_.constEnd() && !*audio},
                                   {QStringLiteral("name"), name},
                                   {QStringLiteral("title"), title},
                                   {QStringLiteral("thumbnail"), thumbnail},
                                   {QStringLiteral("when"), QLocale().toString(when, QLocale::ShortFormat)},
                                   {QStringLiteral("growing"), r.growing}});
        growing = growing || r.growing;
    }
    if (growing) {
        recordings_settle_.start();
    } else {
        recordings_settle_.stop();
    }
    if (list != recordings_) {
        recordings_ = std::move(list);
        emit recordingsChanged();
    }
}

void Session::autosave() {
    if (!editor_ || !dirty() || savedMarker() == autosaved_ || importing() ||
        autosave_job_.state() == oma::JobState::Pending || autosave_job_.state() == oma::JobState::Running) {
        return;
    }
    const auto folder = autosave::dir();
    if (folder.empty()) return;
    if (autosave_file_.isEmpty()) {
        const QString base = project_path_.isEmpty() ? QStringLiteral("untitled") : QFileInfo(project_path_).completeBaseName();
        autosave_file_ = QString::fromStdString(
            (folder / std::format("{}-{}.omamovie", base.toStdString(), QDateTime::currentMSecsSinceEpoch())).string());
    }
    const Marker marker = savedMarker();
    const unsigned generation = generation_;
    const QString file = autosave_file_;
    const QByteArray origin = project_path_.toUtf8();
    autosave_job_ = workers_.submit("autosave", [this, generation, marker, file, origin, doc = document()](oma::JobContext&) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(file.toStdString()).parent_path(), ec);
        auto saved = oma::project::save(doc, std::filesystem::path(file.toStdString()));
        if (QFile sidecar(QString::fromStdString(autosave::origin_of(file.toStdString()).string()));
            saved && sidecar.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            sidecar.write(origin);
        }
        const QString why = saved ? QString() : message(saved.error());
        QMetaObject::invokeMethod(this, [this, generation, marker, file, why] {
            if (generation != generation_ || file != autosave_file_) return; // discarded meanwhile
            if (!why.isEmpty()) {
                oma::log_warn(oma::Category::Project, "autosave failed: {}", why.toStdString());
                return;
            }
            autosaved_ = marker;
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

void Session::discardAutosave() {
    if (!autosave_file_.isEmpty()) autosave::remove(autosave_file_.toStdString());
    autosave_file_.clear();
    autosaved_ = {false, 0, 0, 0, library_revision_};
}

QVariantList Session::recoveredProjects() const {
    QVariantList rows;
    for (const autosave::Recovered& r : autosave::list(autosave::dir())) {
        const QString file = QString::fromStdString(r.file.string());
        if (file == autosave_file_) continue; // this session's own
        rows.append(QVariantMap{{QStringLiteral("file"), file},
                                {QStringLiteral("name"), r.origin.isEmpty() ? QStringLiteral("Untitled project")
                                                                            : QFileInfo(r.origin).completeBaseName()},
                                {QStringLiteral("origin"), r.origin},
                                {QStringLiteral("when"), QLocale().toString(r.when, QLocale::ShortFormat)}});
    }
    return rows;
}

void Session::restoreRecovered(const QString& file) {
    QString origin;
    for (const autosave::Recovered& r : autosave::list(autosave::dir())) {
        if (QString::fromStdString(r.file.string()) == file) origin = r.origin;
    }
    restoring_ = file;
    restoring_origin_ = origin;
    openProject(QUrl::fromLocalFile(file));
    emit recoveredChanged();
}

QVariantList Session::projectVersions(const QString& project) const {
    QVariantList out;
    for (const autosave::Recovered& r : versions::list(project)) {
        out.push_back(QVariantMap{{"file", QString::fromStdString(r.file.string())},
                                  {"when", QLocale().toString(r.when, QLocale::ShortFormat)}});
    }
    return out;
}

void Session::restoreVersion(const QString& file, const QString& project) {
    // Opens like a recovered autosave: the project's own path, the version's content, unsaved.
    // The version file itself stays (saving archives the current file as another version).
    restoring_ = file;
    restoring_origin_ = project;
    restoring_version_ = true;
    openProject(QUrl::fromLocalFile(file));
}

void Session::discardRecovered(const QString& file) {
    if (file == autosave_file_) return;
    autosave::remove(file.toStdString());
    emit recoveredChanged();
}

void Session::setCacheBudget(std::uint64_t bytes_per_store) {
    if (bytes_per_store == thumbnail_cache_.budget() && bytes_per_store == waveform_cache_.budget()) return;
    thumbnail_cache_.set_budget(bytes_per_store);
    waveform_cache_.set_budget(bytes_per_store);
    (void)workers_.submit("cache-evict", [this](oma::JobContext&) {
        for (const oma::DiskCache* cache : {&thumbnail_cache_, &waveform_cache_}) (void)cache->evict();
        return oma::Result<void>{};
    });
}

void Session::clearCache() {
    (void)workers_.submit("cache-clear", [this](oma::JobContext&) {
        std::uint64_t removed = 0;
        for (const oma::DiskCache* cache : {&thumbnail_cache_, &waveform_cache_}) {
            if (auto r = cache->clear()) removed += *r;
        }
        oma::log_info(oma::Category::Cache, "cache cleared: {} bytes", removed);
        QMetaObject::invokeMethod(this, [this] { emit cacheCleared(); }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

QString Session::cacheFolder() const {
    return QString::fromStdString(thumbnail_cache_.dir().parent_path().string());
}

void Session::setRecentProjectsFile(const QString& settings_file) {
    recent_ = std::make_unique<RecentProjects>(settings_file);
    emit recentProjectsChanged();
}

QVariantList Session::recentProjects() const {
    return recent_ ? recent_->rows() : QVariantList{};
}

void Session::forgetRecentProject(const QString& path) {
    if (!recent_) return;
    recent_->remove(path);
    emit recentProjectsChanged();
}

void Session::rememberProject(const QString& path) {
    if (!recent_) return;
    recent_->add(path);
    emit recentProjectsChanged();
}

void Session::openFiles(const QStringList& paths) {
    if (paths.isEmpty()) return;
    if (paths.front().endsWith(QStringLiteral(".omamovie"))) {
        openProject(QUrl::fromLocalFile(paths.front()));
        return;
    }
    newProject();
    queued_files_ = paths;
    importNextQueued(generation_);
}

void Session::importNextQueued(unsigned generation) {
    if (generation != generation_ || queued_files_.isEmpty()) return;
    const QString next = queued_files_.takeFirst();
    // A project file among videos is not media; skip it rather than fail the batch.
    if (next.endsWith(QStringLiteral(".omamovie"))) {
        importNextQueued(generation);
        return;
    }
    importFile(next, true);
}

void Session::open(const QString& path) {
    if (path.endsWith(QStringLiteral(".omamovie"))) {
        openProject(QUrl::fromLocalFile(path));
    } else {
        importFile(path, true);
    }
}

void Session::importFile(const QString& path, bool append, std::optional<oma::project::MediaRef> known) {
    if (path.isEmpty()) return;
    if (!editing_) {
        editing_ = true;
        emit viewChanged();
    }
    setNotice(QStringLiteral("Importing %1…").arg(QFileInfo(path).fileName()));
    const unsigned generation = generation_;
    const tl::MediaId id = known ? known->info.id : tl::MediaId(next_media_++);
    const QString thumbnail_path =
        thumbnails_.filePath(QString::number(id.value()) + QStringLiteral(".png"));
    std::erase_if(imports_, [](const oma::JobHandle& h) {
        const auto s = h.state();
        return s != oma::JobState::Pending && s != oma::JobState::Running;
    });
    // Folders where a moved file may be found again (relink): the project's and the file's own.
    std::vector<std::filesystem::path> roots;
    if (!project_path_.isEmpty()) roots.push_back(std::filesystem::path(project_path_.toStdString()).parent_path());
    roots.push_back(std::filesystem::path(path.toStdString()).parent_path().parent_path());
    roots.insert(roots.end(), relink_roots_.begin(), relink_roots_.end());
    imports_.push_back(workers_.submit("import", [this, generation, id, requested = path, thumbnail_path, append,
                                              known = std::move(known), roots = std::move(roots)](oma::JobContext&) {
        QString source = requested;
        bool relinked = false;
        if (known && !QFileInfo::exists(source)) {
            // An opened project's file moved: the same content by name under nearby folders.
            if (const auto found = oma::project::find_relocated(source.toStdString(), known->fingerprint, roots)) {
                source = QString::fromStdString(found->string());
                relinked = true;
            }
        }
        const std::string file = source.toStdString();
        auto probed = importer_->inspect_input(std::filesystem::path(file));
        if (!probed || (!probed->best_video && !probed->best_audio)) {
            const QString why = probed ? QStringLiteral("no video or audio stream") : message(probed.error());
            // An opened project keeps a missing item in its library (its clips stay on the
            // timeline, shown as gaps) so saving again does not drop it.
            std::optional<LibraryItem> missing;
            if (known) {
                missing.emplace();
                missing->media = known->info;
                missing->path = source;
                missing->name = QString::fromStdString(known->name);
                missing->audio_only = known->audio_only;
                missing->fingerprint = known->fingerprint;
                missing->details.insert("decodePath", QStringLiteral("missing file"));
                missing->missing = true;
            }
            QMetaObject::invokeMethod(this, [this, generation, source, why, missing = std::move(missing)]() mutable {
                if (generation != generation_) return;
                if (missing) {
                    addToLibrary(std::move(*missing), false);
                    fail(QStringLiteral("%1 is missing: find it with Locate… in the library").arg(QFileInfo(source).fileName()));
                } else {
                    fail(QStringLiteral("Cannot import %1: %2").arg(QFileInfo(source).fileName(), why));
                }
                importNextQueued(generation);
            }, Qt::QueuedConnection);
            return oma::Result<void>{};
        }
        // Music, voiceover and sound effects are audio-only media; they go on the audio lanes.
        const bool audio_only = !probed->best_video;
        const auto& stream =
            probed->streams[static_cast<std::size_t>(audio_only ? *probed->best_audio : *probed->best_video)];
        const oma::Rational tb = stream.timebase;
        LibraryItem item;
        item.path = source;
        item.name = QFileInfo(source).fileName();
        item.audio_only = audio_only;
        item.media.id = id;
        item.media.start = stream.start.value_or(*oma::RationalTime::make(0, tb));
        const auto length = stream.duration ? stream.duration : probed->duration;
        if (length) {
            auto in_tb = length->rescaled(item.media.start.timebase(), oma::Rounding::Floor);
            item.media.duration = in_tb.value_or(oma::RationalTime{});
            item.seconds = length->seconds_approx();
        } else {
            item.media.duration = *oma::RationalTime::make(0, item.media.start.timebase());
        }
        item.media.has_video = !audio_only;
        item.media.has_audio = probed->best_audio.has_value();
        item.media.still = stream.video && stream.video->still_image;
        if (item.media.still) item.seconds = static_cast<double>(kStillSeconds);
        item.details.insert("codec", QString::fromStdString(stream.codec));
        if (stream.audio && audio_only) {
            if (stream.audio->sample_rate)
                item.details.insert("resolution", QStringLiteral("%1 Hz").arg(stream.audio->sample_rate->hz()));
            item.details.insert("frameRate", QString::fromStdString(stream.audio->channel_layout));
        }
        if (stream.video) {
            item.rate = stream.video->frame_rate;
            const bool sideways = stream.video->rotation % 180 != 0;
            item.width = static_cast<std::uint32_t>(sideways ? stream.video->height : stream.video->width);
            item.height = static_cast<std::uint32_t>(sideways ? stream.video->width : stream.video->height);
            item.details.insert("resolution",
                                QStringLiteral("%1 × %2").arg(stream.video->width).arg(stream.video->height));
            if (item.rate) {
                const double fps = item.rate->fps().to_double_approx();
                item.details.insert("frameRate",
                                    QString::number(fps, 'f', fps == std::floor(fps) ? 0 : 3) +
                                        (stream.video->variable_frame_rate ? QStringLiteral(" fps (variable)")
                                                                           : QStringLiteral(" fps")));
            }
        }
        item.details.insert("decodePath", audio_only ? QStringLiteral("software (audio)")
                                                     : QStringLiteral("software (preview prototype)"));
        if (known) {
            // The restored timeline references the saved media range; a file standing in for the
            // original (relink) must be the same kind and cover that range, or clips would read
            // past its end. Seconds suffice for this guard; a millisecond absorbs rounding.
            const double needed = known->info.start.seconds_approx() + known->info.duration.seconds_approx();
            const double has = item.media.start.seconds_approx() + item.media.duration.seconds_approx();
            if (audio_only != known->audio_only || (!item.media.still && has + 0.001 < needed)) {
                const QString why = audio_only != known->audio_only
                                        ? QStringLiteral("it is not the same kind of media")
                                        : QStringLiteral("it is shorter than the part the project uses");
                QMetaObject::invokeMethod(this, [this, generation, source, why] {
                    if (generation != generation_) return;
                    setNotice(QStringLiteral("%1 cannot stand in for the missing file: %2").arg(QFileInfo(source).fileName(), why));
                }, Qt::QueuedConnection);
                return oma::Result<void>{};
            }
            item.media = known->info;
            item.name = QString::fromStdString(known->name);
            item.audio_only = known->audio_only;
            item.fingerprint = known->fingerprint;
        } else {
            item.fingerprint = oma::project::fingerprint_file(std::filesystem::path(file)).value_or(oma::project::Fingerprint{});
        }
        // Audio-only media has no picture: the library and the lanes show the sound itself.
        if (!audio_only) {
            // ADR-0009: the cached PNG when this content was seen before, else decode one.
            const std::string key = "thumbnail/v1 256x144 " + media_identity(source, item.fingerprint);
            QFile out(thumbnail_path);
            if (const auto cached = thumbnail_cache_.get(key); cached && out.open(QIODevice::WriteOnly)) {
                out.write(reinterpret_cast<const char*>(cached->data()), static_cast<qint64>(cached->size()));
                out.close();
                item.thumbnail = QUrl::fromLocalFile(thumbnail_path).toString();
            } else if (auto first = frames_.image_at(file, item.media.start)) {
                QByteArray png;
                QBuffer buffer(&png);
                buffer.open(QIODevice::WriteOnly);
                if (first->scaled(256, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG") &&
                    out.open(QIODevice::WriteOnly) && out.write(png) == png.size()) {
                    out.close();
                    item.thumbnail = QUrl::fromLocalFile(thumbnail_path).toString();
                    (void)thumbnail_cache_.put(key, std::span(reinterpret_cast<const std::uint8_t*>(png.constData()),
                                                              static_cast<std::size_t>(png.size())));
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, generation, item = std::move(item), append, relinked]() mutable {
            if (generation != generation_) return;
            if (relinked) {
                oma::log_info(oma::Category::Project, "relinked {} to {}", item.name.toStdString(), item.path.toStdString());
                setNotice(QStringLiteral("Found %1 in its new place").arg(item.name));
            }
            addToLibrary(std::move(item), append);
            importNextQueued(generation);
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

void Session::addToLibrary(LibraryItem item, bool append) {
    // A missing item found again (relink) keeps its place in the library.
    if (const auto it = std::ranges::find_if(library_, [&](const LibraryItem& i) { return i.media.id == item.media.id; });
        it != library_.end()) {
        if (it->path != item.path) ++library_revision_; // the project now points elsewhere: unsaved
        if (item.media.has_audio) {
            waveforms_.request(item.media.id.value(), item.path.toStdString(), media_identity(item.path, item.fingerprint));
        }
        const bool found = it->missing && !item.missing;
        *it = std::move(item);
        if (found) {
            setNotice(QStringLiteral("Found %1").arg(it->name));
            // The open reported the missing files; once none is left, so is the error.
            if (failed_ && std::ranges::none_of(library_, &LibraryItem::missing)) {
                failed_ = false;
                status_.clear();
                emit statusChanged();
            }
        }
        refreshPaths();
        emit libraryChanged();
        emit projectChanged();
        if (editor_) {
            emit sequenceChanged();
            requestFrame();
        }
        return;
    }
    if (editor_ && editor_->timeline().find_media(item.media.id) == nullptr) {
        if (auto r = editor_->add_media(item.media); !r) {
            fail(message(r.error()));
            return;
        }
    }
    if (item.media.has_audio) {
        waveforms_.request(item.media.id.value(), item.path.toStdString(), media_identity(item.path, item.fingerprint));
    }
    library_.push_back(std::move(item));
    refreshPaths();
    selected_media_ = static_cast<int>(library_.size()) - 1;
    notice_.clear();
    emit statusChanged();
    emit libraryChanged();
    // Clips of an opened project show their media's name and pictures once it arrives.
    if (editor_) {
        emit sequenceChanged();
        requestFrame();
    }
    emit selectionChanged();
    if (append) appendSelected();
}

void Session::relinkMedia(int index, const QUrl& url) {
    if (index < 0 || index >= static_cast<int>(library_.size()) || !library_[static_cast<std::size_t>(index)].missing) return;
    const QString path = url.toLocalFile();
    if (path.isEmpty()) return;
    const auto ref = [](const LibraryItem& i) {
        return oma::project::MediaRef{.info = i.media, .path = i.path.toStdString(), .name = i.name.toStdString(),
                                      .audio_only = i.audio_only, .fingerprint = i.fingerprint};
    };
    relink_roots_.push_back(std::filesystem::path(path.toStdString()).parent_path());
    importFile(path, false, ref(library_[static_cast<std::size_t>(index)]));
    // The others moved together, most likely: look for them by name and content there too.
    for (std::size_t i = 0; i < library_.size(); ++i) {
        if (static_cast<int>(i) != index && library_[i].missing) importFile(library_[i].path, false, ref(library_[i]));
    }
}

const Session::LibraryItem* Session::item(tl::MediaId id) const {
    const auto it = std::ranges::find_if(library_, [&](const LibraryItem& i) { return i.media.id == id; });
    return it == library_.end() ? nullptr : &*it;
}

// The sequence takes its frame rate from the first clip placed on it (ui-design §3).
bool Session::ensureSequence(const LibraryItem& first) {
    if (editor_) return true;
    const oma::FrameRate rate = first.rate.value_or(oma::frame_rates::k30);
    auto timeline = tl::Timeline::create(rate, kSequenceAudioRate);
    if (!timeline) timeline = tl::Timeline::create(oma::frame_rates::k30, kSequenceAudioRate);
    if (!timeline) {
        fail(message(timeline.error()));
        return false;
    }
    editor_.emplace(std::move(*timeline));
    if (first.width > 0 && first.height > 0) {
        // Even sizes keep 4:2:0 exports possible later.
        canvas_width_ = (first.width + 1) & ~1U;
        canvas_height_ = (first.height + 1) & ~1U;
    }
    // The timeline owns the canvas (ADR-0010) so that changing it is one undo entry with the
    // offsets it rescales; this first setting is part of the new project (history cleared below).
    (void)editor_->execute(tl::edit::set_canvas(canvas_width_, canvas_height_));
    for (const LibraryItem& i : library_) {
        if (auto r = editor_->add_media(i.media); !r) {
            fail(message(r.error()));
            editor_.reset();
            return false;
        }
    }
    primary_ = editor_->new_track_id();
    if (auto r = editor_->execute(tl::edit::add_track(primary_, tl::TrackKind::Video, "Storyline")); !r) {
        fail(message(r.error()));
        editor_.reset();
        return false;
    }
    editor_->clear_history(); // the storyline is part of the new project, not an edit
    refreshSnapshot();
    return true;
}

std::optional<tl::edit::ClipSource> Session::sourceFor(const LibraryItem& item) const {
    const tl::Timeline& t = editor_->timeline();
    std::optional<std::int64_t> ticks;
    if (item.media.still) {
        ticks = oma::rescale(kStillSeconds, oma::Rational::literal(1, 1), t.timebase(), oma::Rounding::Floor).value_or(0);
    } else {
        // Whole frames of the sequence that fit in the media (rounded down: never past its end).
        auto exact = oma::rescale(item.media.duration.value(), item.media.duration.timebase(), t.timebase(),
                                  oma::Rounding::Floor);
        // Audio is not quantized to the frame grid (CLAUDE.md §6): sound keeps every sample.
        if (exact) ticks = item.audio_only ? *exact : *exact / ticksPerFrame() * ticksPerFrame();
    }
    if (!ticks || *ticks <= 0) return std::nullopt;
    // The source viewer's marks, on the sequence frame grid inside what fits (stills have none).
    std::int64_t from = 0;
    std::int64_t to = *ticks;
    if (!item.media.still) {
        if (item.mark_in) from = std::min(*item.mark_in * ticksPerFrame(), to);
        if (item.mark_out) to = std::min(*item.mark_out * ticksPerFrame(), to);
    }
    if (to <= from) return std::nullopt;
    // source_in = media start + the mark, added in one timebase (CLAUDE.md §6: no implicit
    // conversion): the sequence's when the media start is exact there, else the media's, with
    // the mark rounded to the nearest of its ticks.
    oma::RationalTime source_in = item.media.start;
    if (from > 0) {
        const auto start_here = item.media.start.rescaled(t.timebase(), oma::Rounding::Floor);
        const auto offset = t.at(from).rescaled(item.media.start.timebase(), oma::Rounding::Nearest);
        auto sum = start_here && *start_here == item.media.start ? start_here->plus(t.at(from))
                   : offset ? item.media.start.plus(*offset)
                            : oma::Result<oma::RationalTime>(std::unexpected(offset.error()));
        if (!sum) return std::nullopt;
        source_in = *sum;
    }
    return tl::edit::ClipSource{.media = item.media.id,
                                .source_in = source_in,
                                .duration = t.at(to - from),
                                .time_map = {},
                                .video = {},
                                .audio = {}};
}

// ------------------------------------------------------------------- selection

void Session::selectMedia(int index) {
    if (index < 0 || index >= static_cast<int>(library_.size())) return;
    selected_media_ = index;
    emit selectionChanged();
}

void Session::selectClip(double id) {
    selected_clip_ = tl::ClipId(static_cast<std::uint64_t>(id));
    if (editor_) {
        if (const tl::Clip* c = editor_->timeline().find_clip(selected_clip_)) {
            const auto it = std::ranges::find_if(library_, [&](const LibraryItem& i) { return i.media.id == c->media; });
            if (it != library_.end()) selected_media_ = static_cast<int>(it - library_.begin());
        }
    }
    emit selectionChanged();
}

void Session::selectAdjacentClip(int direction) {
    if (!editor_ || direction == 0) return;
    const tl::Track* track = editor_->timeline().find_track(primary_);
    if (track == nullptr || track->clips.empty()) return;
    const auto& clips = track->clips;
    // Where to step from: the selected storyline clip, else the clip under the playhead (or the
    // last one starting before it), else before the first.
    std::ptrdiff_t at = -1;
    if (const auto it = std::ranges::find(clips, selected_clip_, &tl::Clip::id); it != clips.end()) {
        at = it - clips.begin();
    } else {
        for (std::size_t i = 0; i < clips.size() && clips[i].start_ticks() <= playhead_; ++i)
            at = static_cast<std::ptrdiff_t>(i);
        if (at >= 0 && direction < 0 && clips[static_cast<std::size_t>(at)].start_ticks() < playhead_) ++at;
    }
    const std::ptrdiff_t next =
        std::clamp<std::ptrdiff_t>(at + (direction > 0 ? 1 : -1), 0, static_cast<std::ptrdiff_t>(clips.size()) - 1);
    const tl::Clip& c = clips[static_cast<std::size_t>(next)];
    pause();
    selectClip(static_cast<double>(c.id.value()));
    setFrame(c.start_ticks() / ticksPerFrame());
}

// ------------------------------------------------------------------- edits

bool Session::run(std::unique_ptr<tl::Command> command) {
    if (!editor_) return false;
    pause(); // playback renders a snapshot of the timeline; an edit stops it
    if (auto r = editor_->execute(std::move(command)); !r) {
        setNotice(message(r.error()));
        return false;
    }
    afterEdit();
    return true;
}

void Session::afterEdit() {
    // Undo/redo of a canvas change (ADR-0010) brings the size with it.
    if (editor_->timeline().canvas_width() != 0) {
        canvas_width_ = editor_->timeline().canvas_width();
        canvas_height_ = editor_->timeline().canvas_height();
    }
    refreshSnapshot();
    if (selected_clip_.valid() && editor_->timeline().find_clip(selected_clip_) == nullptr) {
        selected_clip_ = {};
        emit selectionChanged();
    }
    emit sequenceChanged();
    emit selectionChanged(); // the selected clip's properties may have changed
    setFrame(std::min(frame(), lastFrame())); // the content under the playhead may have changed
}

// 0 = append, 1 = insert at the playhead, 2 = overwrite at the playhead.
void Session::placeSelected(int how) {
    if (selected_media_ < 0 || selected_media_ >= static_cast<int>(library_.size())) return;
    const LibraryItem& source = library_[static_cast<std::size_t>(selected_media_)];
    if (!ensureSequence(source)) return;
    const auto clip = sourceFor(source);
    if (!clip) {
        setNotice(QStringLiteral("%1 is too short for one frame").arg(source.name));
        return;
    }
    const tl::ClipId id = editor_->new_clip_id();
    const auto at = editor_->timeline().at(playhead_);
    const bool done = source.audio_only ? placeAudio(how, id, *clip, playhead_)
                      : how == 0        ? run(tl::edit::append(primary_, id, *clip))
                      : how == 1        ? run(tl::edit::insert(primary_, id, at, *clip))
                                        : run(tl::edit::overwrite(primary_, id, at, *clip));
    if (done) {
        selected_clip_ = id;
        emit selectionChanged();
    }
}

// Sound goes below the storyline (ui-design §7.1): append after the first lane's last clip,
// insert as a new connected clip at `start` on the `preferred` lane or else the first lane with
// room (a new lane if none has), overwrite on the first lane. Lanes appear as needed, in the
// same history entry.
bool Session::placeAudio(int how, tl::ClipId id, const tl::edit::ClipSource& clip, std::int64_t start,
                         std::optional<std::size_t> preferred) {
    return placeOnLane(tl::TrackKind::Audio, how, id, clip, start, preferred);
}

// Puts a clip on a lane of `kind` (how 0 appends to the first lane, 1 places it at `start` on
// the preferred or first free lane, else overwrites there), adding a lane when none is free.
// Placed at `start` over a storyline clip, it is connected to it (ADR-0014) in the same edit.
bool Session::placeOnLane(tl::TrackKind kind, int how, tl::ClipId id, const tl::edit::ClipSource& clip,
                          std::int64_t start, std::optional<std::size_t> preferred, const char* name) {
    const tl::Timeline& t = editor_->timeline();
    const auto all = lanes(kind);
    const std::int64_t end = start + clip.duration.value();
    const auto free = [&](tl::TrackId l) {
        return std::ranges::none_of(t.find_track(l)->clips, [&](const tl::Clip& c) {
            return c.start_ticks() < end && start < c.end_ticks();
        });
    };
    std::optional<tl::TrackId> lane;
    if (how == 1) {
        if (preferred && *preferred < all.size() && free(all[*preferred])) lane = all[*preferred];
        for (const tl::TrackId l : all) {
            if (lane) break;
            if (free(l)) lane = l;
        }
    } else if (!all.empty()) {
        lane = all.front();
    }
    std::vector<std::unique_ptr<tl::Command>> steps;
    if (!lane) {
        lane = editor_->new_track_id();
        steps.push_back(tl::edit::add_track(*lane, kind, laneName(kind, all.size())));
    }
    steps.push_back(how == 0 ? tl::edit::append(*lane, id, clip) : tl::edit::overwrite(*lane, id, t.at(start), clip));
    if (how == 1) {
        if (const tl::Clip* primary = t.clip_at(primary_, start); primary != nullptr && primary->time_map.is_constant()) {
            steps.push_back(tl::edit::connect(id, primary->id));
        }
    }
    const bool audio = kind == tl::TrackKind::Audio;
    return run(tl::edit::transaction(name != nullptr ? name
                                     : how == 0 ? (audio ? "Append Audio" : "Append")
                                     : how == 1 ? (audio ? "Connect Audio" : "Connect")
                                                : (audio ? "Overwrite Audio" : "Overwrite"),
                                     std::move(steps)));
}

std::string Session::laneName(tl::TrackKind kind, std::size_t existing) {
    return (kind == tl::TrackKind::Audio ? "Audio " : "Video ") + std::to_string(existing + 1);
}

std::vector<tl::TrackId> Session::lanes(tl::TrackKind kind) const {
    std::vector<tl::TrackId> out;
    if (!editor_) return out;
    for (const tl::Track& track : editor_->timeline().tracks()) {
        if (track.kind == kind && track.id != primary_) out.push_back(track.id);
    }
    return out;
}

std::vector<tl::TrackId> Session::audioLanes() const {
    return lanes(tl::TrackKind::Audio);
}

void Session::appendSelected() {
    placeSelected(0);
}

void Session::insertSelected() {
    placeSelected(1);
}

void Session::overwriteSelected() {
    placeSelected(2);
}

void Session::splitAtPlayhead() {
    if (!editor_) return;
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* c = nullptr;
    if (const tl::Clip* selected = t.find_clip(selected_clip_);
        selected != nullptr && selected->start_ticks() <= playhead_ && playhead_ < selected->end_ticks()) {
        c = selected;
    } else {
        c = t.clip_at(primary_, playhead_);
    }
    if (c == nullptr || c->start_ticks() == playhead_) {
        setNotice(QStringLiteral("Move the playhead inside a clip to split it"));
        return;
    }
    const tl::ClipId left = c->id;
    if (run(tl::edit::split(left, editor_->timeline().at(playhead_)))) {
        selected_clip_ = left;
        emit selectionChanged();
    }
}

std::unique_ptr<tl::Command> Session::deleteCommand(bool ripple) const {
    if (!editor_) return nullptr;
    tl::ClipId id = selected_clip_;
    if (!id.valid()) {
        const tl::Clip* c = editor_->timeline().clip_at(primary_, playhead_);
        if (c == nullptr) return nullptr;
        id = c->id;
    }
    const tl::Track* track = editor_->timeline().track_of(id);
    if (track == nullptr) return nullptr;
    if (track->id == primary_) return ripple ? tl::edit::ripple_delete(id) : tl::edit::remove_clip(id);
    // Lanes below and above the storyline are not magnetic: deleting leaves the time free.
    std::vector<std::unique_ptr<tl::Command>> steps;
    steps.push_back(tl::edit::remove_clip(id));
    if (track->clips.size() == 1) steps.push_back(tl::edit::remove_track(track->id));
    return tl::edit::transaction("Delete", std::move(steps));
}

void Session::deleteSelected(bool ripple) {
    clearEditScope();
    if (auto command = deleteCommand(ripple)) run(std::move(command));
}

void Session::previewDelete(bool ripple) { previewCommand(deleteCommand(ripple)); }

void Session::previewTrim(double id, bool head, int frames) {
    previewCommand(frames == 0 ? nullptr : trimCommand(tl::ClipId(static_cast<std::uint64_t>(id)), head, frames));
}

void Session::clearEditScope() {
    if (edit_scope_.isEmpty()) return;
    edit_scope_.clear();
    emit editScopeChanged();
}

void Session::previewCommand(std::unique_ptr<tl::Command> command) {
    if (!editor_ || !command) {
        clearEditScope();
        return;
    }
    // ponytail: copies the timeline per preview (O(clips)); fine for a hover or a drag step at
    // the long-form gate's 5,000 clips. Keep the copy if previews ever run every frame.
    const tl::Timeline& before = editor_->timeline();
    tl::Editor trial{tl::Timeline(before)};
    if (!trial.execute(std::move(command))) {
        clearEditScope();
        return;
    }
    std::unordered_map<std::uint64_t, std::int64_t> after;
    for (const tl::Track& track : trial.timeline().tracks()) {
        for (const tl::Clip& c : track.clips) after.emplace(c.id.value(), c.start_ticks());
    }
    QVariantMap moved;
    QVariantList removed;
    for (const tl::Track& track : before.tracks()) {
        for (const tl::Clip& c : track.clips) {
            const auto it = after.find(c.id.value());
            const QString key = QString::number(c.id.value());
            if (it == after.end()) {
                removed.push_back(static_cast<double>(c.id.value()));
            } else if (it->second != c.start_ticks()) {
                moved.insert(key, it->second < c.start_ticks() ? -1 : 1);
            }
        }
    }
    int markers = 0;
    for (const tl::Marker& m : before.markers()) {
        const tl::Marker* now = trial.timeline().find_marker(m.id);
        markers += now == nullptr || now->time != m.time ? 1 : 0;
    }
    edit_scope_ = {{"active", true}, {"moved", moved}, {"removed", removed}, {"markers", markers}};
    emit editScopeChanged();
}

std::unique_ptr<tl::Command> Session::trimCommand(tl::ClipId clip, bool head, int frames) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(clip) : nullptr;
    if (c == nullptr) return nullptr;
    const std::int64_t delta = static_cast<std::int64_t>(frames) * ticksPerFrame();
    const tl::Timeline& t = editor_->timeline();
    const bool magnetic = t.track_of(clip) != nullptr && t.track_of(clip)->id == primary_;
    return head ? tl::edit::trim_start(clip, t.at(c->start_ticks() + delta), magnetic)
                : tl::edit::trim_end(clip, t.at(c->end_ticks() + delta), magnetic);
}

void Session::trimClip(double id, bool head, int frames) {
    clearEditScope();
    if (!editor_ || frames == 0) return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    auto command = trimCommand(clip, head, frames);
    if (command && run(std::move(command))) {
        selected_clip_ = clip;
        emit selectionChanged();
    }
}

void Session::moveClip(double id, int lanes, int frames) {
    if (!editor_ || (lanes == 0 && frames == 0)) return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* c = t.find_clip(clip);
    const tl::Track* from = t.track_of(clip);
    if (c == nullptr || from == nullptr || from->id == primary_) return; // the storyline reorders by editing
    const auto all = this->lanes(from->kind);
    const auto index = std::ranges::find(all, from->id) - all.begin();
    const auto target = std::max<std::ptrdiff_t>(0, index + lanes);
    const std::int64_t start = std::max<std::int64_t>(0, c->start_ticks() + (static_cast<std::int64_t>(frames) * ticksPerFrame()));
    std::vector<std::unique_ptr<tl::Command>> steps;
    tl::TrackId to;
    if (target < static_cast<std::ptrdiff_t>(all.size())) {
        to = all[static_cast<std::size_t>(target)];
    } else {
        to = editor_->new_track_id();
        steps.push_back(tl::edit::add_track(to, from->kind, laneName(from->kind, all.size())));
    }
    if (to == from->id && start == c->start_ticks()) return;
    steps.push_back(tl::edit::move_clip(clip, to, t.at(start)));
    if (to != from->id && from->clips.size() == 1) steps.push_back(tl::edit::remove_track(from->id));
    if (run(tl::edit::transaction("Move", std::move(steps)))) {
        selected_clip_ = clip;
        emit selectionChanged();
    }
}

std::int64_t Session::ticksAt(double seconds) const {
    // Display seconds to the nearest frame of the sequence grid, as seek() does.
    return std::max<std::int64_t>(0, std::llround(seconds * frameRate())) * ticksPerFrame();
}

// Candidates are time zero and the edges of the other storyline clips; the moving clip's own
// edges count as staying where it is.
std::int64_t Session::nearestCut(std::int64_t ticks, tl::ClipId moving) const {
    std::int64_t best = 0;
    const auto consider = [&](std::int64_t cut) {
        if (std::abs(cut - ticks) < std::abs(best - ticks)) best = cut;
    };
    if (const tl::Track* track = editor_->timeline().find_track(primary_)) {
        for (const tl::Clip& c : track->clips) {
            if (c.id == moving) continue;
            consider(c.start_ticks());
            consider(c.end_ticks());
        }
        if (const tl::Clip* m = editor_->timeline().find_clip(moving)) {
            consider(m->start_ticks());
            consider(m->end_ticks());
        }
    }
    return best;
}

double Session::storylineCut(double seconds, double moving) const {
    if (!editor_ || !std::isfinite(seconds)) return 0;
    const tl::ClipId clip(static_cast<std::uint64_t>(moving));
    return editor_->timeline().at(nearestCut(ticksAt(seconds), clip)).seconds_approx();
}

QVariantMap Session::snapSpan(double seconds, double length, double exclude, double tolerance) const {
    double start = seconds;
    double line = -1;
    if (editor_ && std::isfinite(seconds) && std::isfinite(length)) {
        const tl::Timeline& t = editor_->timeline();
        const tl::ClipId skip(static_cast<std::uint64_t>(exclude));
        double reach = tolerance;
        const auto consider = [&](double edge) {
            for (const double offset : {0.0, length}) { // either end of the span
                const double d = std::abs(edge - (seconds + offset));
                if (d <= reach) {
                    reach = d;
                    start = edge - offset;
                    line = edge;
                }
            }
        };
        consider(0);
        consider(position());
        // ponytail: linear scan of every edge per pointer move; fine for hundreds of clips,
        // a sorted edge list with binary search if projects grow to thousands.
        for (const tl::Track& track : t.tracks()) {
            for (const tl::Clip& c : track.clips) {
                if (c.id == skip) continue;
                consider(t.at(c.start_ticks()).seconds_approx());
                consider(t.at(c.end_ticks()).seconds_approx());
            }
        }
    }
    return {{QStringLiteral("start"), start}, {QStringLiteral("line"), line}};
}

void Session::reorderClip(double id, double seconds) {
    if (!editor_ || !std::isfinite(seconds)) return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* c = t.find_clip(clip);
    if (c == nullptr || t.track_of(clip)->id != primary_) return;
    const std::int64_t cut = nearestCut(ticksAt(seconds), clip);
    if (cut == c->start_ticks() || cut == c->end_ticks()) return; // dropped where it already is
    // ripple_move measures the landing point with the clip taken out.
    const std::int64_t at = cut >= c->end_ticks() ? cut - c->duration.value() : cut;
    if (run(tl::edit::ripple_move(clip, t.at(at)))) {
        selected_clip_ = clip;
        emit selectionChanged();
    }
}

void Session::dropMedia(int index, double seconds, int lane) {
    if (index < 0 || index >= static_cast<int>(library_.size()) || !std::isfinite(seconds)) return;
    selected_media_ = index;
    const LibraryItem& source = library_[static_cast<std::size_t>(index)];
    if (!ensureSequence(source)) return;
    const auto clip = sourceFor(source);
    if (!clip) {
        setNotice(QStringLiteral("%1 is too short for one frame").arg(source.name));
        return;
    }
    const tl::ClipId id = editor_->new_clip_id();
    const std::int64_t at = ticksAt(seconds);
    // `lane`: -1 the storyline, 0.. the audio lanes below it, -2 and down the video lanes above it.
    const bool done =
        source.audio_only
            ? placeAudio(1, id, *clip, at, lane >= 0 ? std::optional<std::size_t>(lane) : std::nullopt)
        : lane <= -2
            ? placeOnLane(tl::TrackKind::Video, 1, id, *clip, at, static_cast<std::size_t>(-2 - lane))
            : run(tl::edit::insert(primary_, id, editor_->timeline().at(nearestCut(at, {})), *clip));
    if (done) selected_clip_ = id;
    emit selectionChanged();
}

void Session::setClipSpeed(int num, int den) {
    if (!editor_ || num <= 0 || den <= 0) return;
    const tl::Track* track = editor_->timeline().track_of(selected_clip_);
    auto speed = oma::Rational::make(num, den);
    if (track == nullptr || !speed) return;
    if (run(tl::edit::set_speed(selected_clip_, *speed, track->id == primary_)) && num != den) {
        setNotice(QStringLiteral("Sound plays only at normal speed"));
    }
}

void Session::freezeFrame(double seconds) {
    if (!editor_ || !std::isfinite(seconds) || seconds <= 0) return;
    const tl::Timeline& t = editor_->timeline();
    // The selected clip if the playhead is on it, else the storyline clip there.
    const tl::Clip* c = t.find_clip(selected_clip_);
    if (c == nullptr || playhead_ < c->start_ticks() || playhead_ >= c->end_ticks()) c = t.clip_at(primary_, playhead_);
    if (c == nullptr) {
        setNotice(QStringLiteral("Move the playhead onto a clip to freeze a frame"));
        return;
    }
    const tl::ClipId id = c->id;
    const bool ripple = t.track_of(id)->id == primary_;
    if (run(tl::edit::freeze_frame(id, t.at(playhead_), t.at(ticksAt(seconds)), ripple))) {
        selected_clip_ = id;
        emit selectionChanged();
    }
}

void Session::setSpeedRamp(int preset) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || preset < 0 || preset > 2) {
        setNotice(QStringLiteral("Select a clip to ramp its speed"));
        return;
    }
    using Kind = tl::TimeSegment::Kind;
    const auto q = [](std::int64_t n, std::int64_t d) { return oma::Rational::make(n, d).value(); };
    const std::int64_t tpf = ticksPerFrame();
    const std::int64_t frames = (c->end_ticks() - c->start_ticks()) / tpf;
    std::vector<tl::TimeSegment> segments;
    if (preset < 2) {
        // Average speed 1×: the same media over the same duration.
        if (frames < 2) return;
        const auto slow = q(1, 2);
        const auto fast = q(3, 2);
        segments.push_back({.kind = Kind::Ramp, .length = frames * tpf, .from = preset == 0 ? slow : fast,
                            .to = preset == 0 ? fast : slow});
    } else {
        // Up and down at an average of 1½×: two thirds of the frames cover the same media.
        const std::int64_t half = std::max<std::int64_t>(1, (frames * 2 / 3) / 2);
        segments.push_back({.kind = Kind::Ramp, .length = half * tpf, .from = q(1, 1), .to = q(2, 1)});
        segments.push_back({.kind = Kind::Ramp, .length = half * tpf, .from = q(2, 1), .to = q(1, 1)});
    }
    auto map = tl::TimeMap::segmented(std::move(segments));
    if (!map) return;
    const bool ripple = editor_->timeline().track_of(c->id)->id == primary_;
    if (run(tl::edit::set_time_map(c->id, std::move(*map), ripple))) {
        setNotice(QStringLiteral("Speed ramps play without sound"));
    }
}

void Session::reverseClip() {
    if (!editor_ || editor_->timeline().find_clip(selected_clip_) == nullptr) {
        setNotice(QStringLiteral("Select a clip to reverse"));
        return;
    }
    if (run(tl::edit::reverse(selected_clip_))) setNotice(QStringLiteral("Reversed clips play without sound"));
}

void Session::detachAudio() {
    if (!editor_) return;
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* c = t.find_clip(selected_clip_);
    if (c == nullptr || t.track_of(c->id)->id != primary_) c = t.clip_at(primary_, playhead_);
    const tl::MediaInfo* m = c != nullptr ? t.find_media(c->media) : nullptr;
    if (c == nullptr || m == nullptr || !m->has_audio || c->audio_detached) {
        setNotice(QStringLiteral("Select a storyline clip with sound to detach it"));
        return;
    }
    const tl::ClipId video = c->id;
    const std::int64_t start = c->start_ticks();
    const std::int64_t end = c->end_ticks();
    const auto lanes = audioLanes();
    std::optional<tl::TrackId> lane;
    for (const tl::TrackId l : lanes) {
        if (std::ranges::none_of(t.find_track(l)->clips, [&](const tl::Clip& other) {
                return other.start_ticks() < end && start < other.end_ticks();
            })) {
            lane = l;
            break;
        }
    }
    std::vector<std::unique_ptr<tl::Command>> steps;
    if (!lane) {
        lane = editor_->new_track_id();
        steps.push_back(tl::edit::add_track(*lane, tl::TrackKind::Audio, "Audio " + std::to_string(lanes.size() + 1)));
    }
    const tl::ClipId sound = editor_->new_clip_id();
    steps.push_back(tl::edit::detach_audio(video, *lane, sound));
    if (run(tl::edit::transaction("Detach Audio", std::move(steps)))) {
        selected_clip_ = sound;
        emit selectionChanged();
    }
}

void Session::setClipAudio(double gain, double fadeIn, double fadeOut, bool muted) {
    if (!editor_ || !selected_clip_.valid()) return;
    const tl::Clip* c = editor_->timeline().find_clip(selected_clip_);
    if (c == nullptr || !std::isfinite(gain) || !std::isfinite(fadeIn) || !std::isfinite(fadeOut)) return;
    const std::int64_t frames = c->duration.value() / ticksPerFrame();
    const std::int64_t in = std::clamp<std::int64_t>(std::llround(fadeIn * frameRate()), 0, frames);
    const std::int64_t out = std::clamp<std::int64_t>(std::llround(fadeOut * frameRate()), 0, frames - in);
    tl::AudioProperties audio = c->audio;
    audio.gain = static_cast<float>(std::clamp(gain, 0.0, 4.0));
    audio.fade_in = editor_->timeline().at(in * ticksPerFrame());
    audio.fade_out = editor_->timeline().at(out * ticksPerFrame());
    audio.muted = muted;
    if (audio == c->audio) return;
    const tl::ClipId id = c->id;
    run(tl::edit::set_audio(id, audio));
}

void Session::setSelectedAudio(const tl::AudioProperties& audio) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || audio == c->audio) return;
    const tl::ClipId id = c->id;
    run(tl::edit::set_audio(id, audio));
}

void Session::setClipEq(double low, double mid, double high) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(low) || !std::isfinite(mid) || !std::isfinite(high)) return;
    tl::AudioProperties audio = c->audio;
    const auto band = [](double db) { return static_cast<float>(std::clamp(std::round(db * 2.0) / 2.0, -24.0, 24.0)); };
    audio.eq = tl::Equalizer{.low_db = band(low), .mid_db = band(mid), .high_db = band(high)};
    setSelectedAudio(audio);
}

std::optional<Session::ClipSound> Session::selectedSound() const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return std::nullopt;
    auto waveform = waveforms_.get(c->media.value());
    if (!waveform) return std::nullopt;
    // Display-grade seconds are enough to pick buckets of 10 ms (CLAUDE.md §6).
    const double from = c->source_in.seconds_approx() - waveform->start.seconds_approx();
    const double length = c->duration.seconds_approx() * c->time_map.speed().to_double_approx();
    return ClipSound{.waveform = std::move(waveform), .from = from, .to = from + length};
}

void Session::setClipNoise(double amount) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(amount)) return;
    tl::AudioProperties audio = c->audio;
    audio.noise.amount = static_cast<float>(std::clamp(amount, 0.0, 1.0));
    if (audio.noise.amount > 0.0F && c->audio.noise.amount == 0.0F) {
        // Measure the noise once, when reduction is turned on; later changes keep it.
        const auto sound = selectedSound();
        if (!sound) {
            setNotice(QStringLiteral("Still analysing this clip's sound; try again in a moment"));
            return;
        }
        const auto floor = sound->waveform->noise_floor_db(sound->from, sound->to);
        if (!floor) {
            setNotice(QStringLiteral("This clip is silent: there is no noise to reduce"));
            return;
        }
        audio.noise.floor_db = std::clamp(*floor, -120.0F, 0.0F);
    }
    setSelectedAudio(audio);
}

void Session::normalizeClip() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    const auto sound = selectedSound();
    if (!sound) {
        setNotice(QStringLiteral("Still analysing this clip's sound; try again in a moment"));
        return;
    }
    const float peak = sound->waveform->peak_between(sound->from, sound->to);
    if (peak <= 0.0F) {
        setNotice(QStringLiteral("This clip is silent"));
        return;
    }
    // Peak normalization to -1 dBFS, before the equalizer; loudness (LUFS) matching would need a
    // loudness meter the project does not have yet.
    tl::AudioProperties audio = c->audio;
    audio.gain = static_cast<float>(std::clamp(std::pow(10.0, -1.0 / 20.0) / static_cast<double>(peak), 0.0, 4.0));
    audio.muted = false;
    setSelectedAudio(audio);
    if (audio.gain >= 4.0F) setNotice(QStringLiteral("The clip is very quiet: volume raised to the maximum"));
}

namespace {

// Lift/gamma/gain wheels <-> ASC CDL (ADR-0012). A wheel's tint (x, y) spreads over the channels
// at 0°, 120° and 240° (red, green, blue), summing to zero, on top of its level:
//   lift_c  = 0.2 level + 0.1 tint_c           (added in the shadows)
//   gamma_c = 2^-(level + 0.5 tint_c)          (the CDL power)
//   gain_c  = 2^(level + 0.5 tint_c)
// and lift/gain become CDL exactly: slope = gain (1 - lift), offset = gain lift. Every wheel in
// range lands inside the CDL's ranges (slope ≤ 3.7, |offset| ≤ 0.85, power in [0.35, 2.83]).
struct Wheel {
    double x = 0.0;
    double y = 0.0;
    double level = 0.0;
};
constexpr std::array<double, 3> kWheelAngles{0.0, 2.0 * std::numbers::pi / 3.0, 4.0 * std::numbers::pi / 3.0};

std::array<double, 3> spread(const Wheel& w, double level_scale, double tint_scale) {
    std::array<double, 3> v{};
    for (std::size_t c = 0; c < 3; ++c)
        v[c] = (level_scale * w.level) + (tint_scale * ((w.x * std::cos(kWheelAngles[c])) + (w.y * std::sin(kWheelAngles[c]))));
    return v;
}

Wheel gather(const std::array<double, 3>& v, double level_scale, double tint_scale) {
    const double mean = (v[0] + v[1] + v[2]) / 3.0;
    Wheel w{.x = 0.0, .y = 0.0, .level = mean / level_scale};
    for (std::size_t c = 0; c < 3; ++c) {
        const double d = (v[c] - mean) / tint_scale;
        w.x += d * std::cos(kWheelAngles[c]) / 1.5; // sum of cos² over the three angles is 1.5
        w.y += d * std::sin(kWheelAngles[c]) / 1.5;
    }
    return w;
}

Wheel wheel_from(const QVariant& value) {
    const QVariantMap m = value.toMap();
    Wheel w{.x = m.value("x").toDouble(), .y = m.value("y").toDouble(), .level = m.value("level").toDouble()};
    if (!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.level)) return {};
    const double r = std::hypot(w.x, w.y);
    if (r > 1.0) {
        w.x /= r;
        w.y /= r;
    }
    w.level = std::clamp(w.level, -1.0, 1.0);
    return w;
}

QVariantMap to_variant(const Wheel& w) {
    return {{"x", w.x}, {"y", w.y}, {"level", w.level}};
}

} // namespace

QVariantMap Session::wheels_of(const tl::Cdl& cdl) {
    std::array<double, 3> lift{};
    std::array<double, 3> gain{};
    std::array<double, 3> gamma{};
    for (std::size_t c = 0; c < 3; ++c) {
        const double g = cdl.slope[c] + cdl.offset[c];
        gain[c] = std::log2(std::max(g, 1e-6));
        lift[c] = g > 1e-6 ? cdl.offset[c] / g : 0.0;
        gamma[c] = -std::log2(cdl.power[c]);
    }
    return {{"lift", to_variant(gather(lift, 0.2, 0.1))},
            {"gamma", to_variant(gather(gamma, 1.0, 0.5))},
            {"gain", to_variant(gather(gain, 1.0, 0.5))}};
}

void Session::setClipWheels(const QVariantMap& wheels) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    const auto lift = spread(wheel_from(wheels.value("lift")), 0.2, 0.1);
    const auto gamma = spread(wheel_from(wheels.value("gamma")), 1.0, 0.5);
    const auto gain = spread(wheel_from(wheels.value("gain")), 1.0, 0.5);
    // Four decimals: wheel noise does not make a new history entry.
    const auto snap = [](double v) { return std::round(v * 1e4) / 1e4; };
    tl::VideoProperties video = c->video;
    for (std::size_t ch = 0; ch < 3; ++ch) {
        const double g = std::exp2(gain[ch]);
        video.grade.cdl.slope[ch] = snap(g * (1.0 - lift[ch]));
        video.grade.cdl.offset[ch] = snap(g * lift[ch]);
        video.grade.cdl.power[ch] = snap(std::exp2(-gamma[ch]));
    }
    setSelectedVideo(video);
}

namespace {

std::vector<tl::CurvePoint>* curve_of(tl::VideoProperties& video, int channel) {
    switch (channel) {
    case 0: return &video.grade.curves.master;
    case 1: return &video.grade.curves.red;
    case 2: return &video.grade.curves.green;
    case 3: return &video.grade.curves.blue;
    default: return nullptr;
    }
}

constexpr double kCurveGap = 0.01; // the least x distance between neighbouring points

} // namespace

void Session::setSelectedCurve(int channel, std::vector<tl::CurvePoint> points) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    std::vector<tl::CurvePoint>* curve = curve_of(video, channel);
    if (curve == nullptr) return;
    // A straight diagonal is no curve at all.
    if (points.size() == 2 && points[0] == tl::CurvePoint{} && points[1] == tl::CurvePoint{.x = 1.0, .y = 1.0}) points.clear();
    *curve = std::move(points);
    setSelectedVideo(video);
}

void Session::addCurvePoint(int channel, double x, double y) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(x) || !std::isfinite(y)) return;
    tl::VideoProperties video = c->video;
    const std::vector<tl::CurvePoint>* current = curve_of(video, channel);
    if (current == nullptr) return;
    std::vector<tl::CurvePoint> points = *current;
    if (points.empty()) points = {{.x = 0.0, .y = 0.0}, {.x = 1.0, .y = 1.0}};
    if (points.size() >= tl::kMaxCurvePoints) {
        setNotice(QStringLiteral("A curve holds at most %1 points").arg(tl::kMaxCurvePoints));
        return;
    }
    const tl::CurvePoint p{.x = std::clamp(x, 0.0, 1.0), .y = std::clamp(y, 0.0, 1.0)};
    const auto at = std::ranges::lower_bound(points, p.x, {}, &tl::CurvePoint::x);
    if ((at != points.end() && at->x - p.x < kCurveGap) || (at != points.begin() && p.x - std::prev(at)->x < kCurveGap)) return;
    points.insert(at, p);
    setSelectedCurve(channel, std::move(points));
}

void Session::moveCurvePoint(int channel, int index, double x, double y) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(x) || !std::isfinite(y)) return;
    tl::VideoProperties video = c->video;
    const std::vector<tl::CurvePoint>* current = curve_of(video, channel);
    if (current == nullptr || index < 0 || index >= static_cast<int>(current->size())) return;
    std::vector<tl::CurvePoint> points = *current;
    const auto i = static_cast<std::size_t>(index);
    const double lo = i == 0 ? 0.0 : points[i - 1].x + kCurveGap;
    const double hi = i + 1 == points.size() ? 1.0 : points[i + 1].x - kCurveGap;
    points[i] = {.x = std::round(std::clamp(x, lo, hi) * 1e4) / 1e4, .y = std::round(std::clamp(y, 0.0, 1.0) * 1e4) / 1e4};
    setSelectedCurve(channel, std::move(points));
}

void Session::removeCurvePoint(int channel, int index) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    const std::vector<tl::CurvePoint>* current = curve_of(video, channel);
    if (current == nullptr || index <= 0 || index + 1 >= static_cast<int>(current->size())) return; // ends stay
    std::vector<tl::CurvePoint> points = *current;
    points.erase(points.begin() + index);
    setSelectedCurve(channel, std::move(points));
}

QVariantList Session::curveSamples(int channel, int count) const {
    QVariantList out;
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || count < 2 || count > 1024) return out;
    tl::VideoProperties video = c->video;
    const std::vector<tl::CurvePoint>* curve = curve_of(video, channel);
    if (curve == nullptr) return out;
    std::vector<oma::compositor::CurvePoint> points;
    for (const tl::CurvePoint& p : *curve) points.push_back({.x = p.x, .y = p.y});
    for (int i = 0; i < count; ++i)
        out.push_back(oma::compositor::evaluate_curve(points, static_cast<double>(i) / (count - 1)));
    return out;
}

QVariantList Session::luts() const {
    QVariantList out;
    if (!editor_) return out;
    for (const tl::LutInfo& l : editor_->timeline().luts())
        out.push_back(QVariantMap{{"id", static_cast<double>(l.id.value())}, {"name", QString::fromStdString(l.name)}});
    return out;
}

void Session::importLut(const QUrl& url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!editor_ || path.isEmpty()) return;
    const QFileInfo info(path);
    if (info.size() > static_cast<qint64>(oma::compositor::kMaxCubeBytes)) {
        setNotice(QStringLiteral("%1 is too large for a LUT").arg(info.fileName()));
        return;
    }
    const unsigned generation = generation_;
    const tl::ClipId clip = selected_clip_;
    imports_.push_back(workers_.submit("lut-import", [this, generation, path, clip](oma::JobContext&) {
        QFile file(path);
        std::optional<oma::Result<oma::compositor::Lut3d>> parsed;
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.read(static_cast<qint64>(oma::compositor::kMaxCubeBytes) + 1);
            parsed = oma::compositor::parse_cube(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        }
        auto table = parsed && *parsed ? std::make_shared<const oma::compositor::Lut3d>(std::move(**parsed)) : nullptr;
        const QString why = !parsed ? QStringLiteral("cannot read the file") : !*parsed ? message(parsed->error()) : QString();
        QMetaObject::invokeMethod(this, [this, generation, path, clip, table = std::move(table), why] {
            if (generation != generation_ || !editor_) return;
            const QString name = QFileInfo(path).completeBaseName();
            if (!table) {
                setNotice(QStringLiteral("Cannot use %1: %2").arg(name, why));
                return;
            }
            const tl::LutId id(next_lut_++);
            if (auto r = editor_->add_lut({.id = id, .name = name.toStdString()}); !r) {
                setNotice(message(r.error()));
                return;
            }
            lut_paths_[id.value()] = path;
            auto tables = std::make_shared<LutTables>(*luts_);
            tables->emplace(id.value(), table);
            luts_ = std::move(tables);
            emit lutsChanged();
            if (clip == selected_clip_) setClipLut(static_cast<double>(id.value()), 1.0);
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

// ------------------------------------------------------------------- project files (ADR-0007)

Session::Marker Session::savedMarker() const {
    return {editor_.has_value(), editor_ ? editor_->revision() : 0, library_.size(), lut_paths_.size(), library_revision_};
}

QString Session::projectName() const {
    return project_path_.isEmpty() ? QStringLiteral("Untitled project") : QFileInfo(project_path_).completeBaseName();
}

oma::project::Document Session::document() const {
    oma::project::Document doc;
    for (const LibraryItem& i : library_) {
        doc.media.push_back({.info = i.media,
                             .path = std::filesystem::path(i.path.toStdString()),
                             .name = i.name.toStdString(),
                             .audio_only = i.audio_only,
                             .fingerprint = i.fingerprint});
    }
    if (editor_) {
        for (const tl::LutInfo& l : editor_->timeline().luts()) {
            const auto file = lut_paths_.find(l.id.value());
            doc.luts.push_back({.info = l, .path = file != lut_paths_.end() ? file->second.toStdString() : std::string()});
        }
        doc.timeline = editor_->timeline(); // a copy: the save runs on the job worker
        doc.storyline = primary_;
    }
    doc.canvas_width = canvas_width_;
    doc.canvas_height = canvas_height_;
    return doc;
}

bool Session::importing() const {
    return std::ranges::any_of(imports_, [](const oma::JobHandle& h) {
        const auto st = h.state();
        return st == oma::JobState::Pending || st == oma::JobState::Running;
    });
}

void Session::saveProject(const QUrl& url) {
    QString path = url.isEmpty() ? project_path_ : (url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".omamovie"))) path += QStringLiteral(".omamovie");
    // The library is complete only once its imports land (an opened project re-imports all).
    if (std::ranges::any_of(imports_, [](const oma::JobHandle& h) {
            const auto st = h.state();
            return st == oma::JobState::Pending || st == oma::JobState::Running;
        })) {
        setNotice(QStringLiteral("Wait for the import to finish, then save"));
        return;
    }
    oma::project::Document doc = document();
    const Marker marker = savedMarker();
    const unsigned generation = generation_;
    setNotice(QStringLiteral("Saving…"));
    save_job_ = workers_.submit("save-project", [this, generation, path, marker, doc = std::move(doc)](oma::JobContext&) {
        // The file about to be replaced becomes a prior version (mistaken-edit recovery).
        if (!versions::archive(path)) {
            oma::log_warn(oma::Category::Project, "could not keep the previous version of {}", path.toStdString());
        }
        auto saved = oma::project::save(doc, std::filesystem::path(path.toStdString()));
        const QString why = saved ? QString() : message(saved.error());
        QMetaObject::invokeMethod(this, [this, generation, path, marker, why] {
            if (generation != generation_) return;
            if (!why.isEmpty()) {
                fail(QStringLiteral("Cannot save %1: %2").arg(QFileInfo(path).fileName(), why));
                return;
            }
            project_path_ = path;
            saved_ = marker; // edits made while saving keep the project dirty
            rememberProject(path);
            if (!dirty()) discardAutosave(); // the file now holds everything the autosave did
            setNotice(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
            emit projectChanged();
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

void Session::exportMovie(const QUrl& url) {
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty() || !editor_ || export_progress_ >= 0) return;
    if (!path.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) path += QStringLiteral(".mp4");
    if (std::ranges::any_of(library_, &LibraryItem::missing)) {
        setNotice(QStringLiteral("Locate the missing files first: export would leave gaps where they play"));
        return;
    }
    // Every clip's media must be loaded (an opened project re-imports its library; a finished
    // import job's result may still be on its way to this thread), or it would render as a gap.
    for (const tl::Track& track : editor_->timeline().tracks()) {
        for (const tl::Clip& c : track.clips) {
            if (!c.title && !paths_->contains(c.media.value())) {
                setNotice(QStringLiteral("Wait for the media to finish loading, then export"));
                return;
            }
        }
    }
    if (!tl::unknown_effects(editor_->timeline()).empty()) {
        // ADR-0016: never render a different movie silently.
        setNotice(QStringLiteral("Some effects come from a newer OmaMovie: remove or disable them before exporting"));
        return;
    }
    ExportRequest request{.timeline = editor_->timeline(), .paths = *paths_, .luts = *luts_,
                          .file = std::filesystem::path(path.toStdString()), .width = canvas_width_ & ~1U,
                          .height = canvas_height_ & ~1U, .frames = lastFrame() + 1, .ticks_per_frame = ticksPerFrame(),
                          .audio_rate = kSequenceAudioRate,
                          .encoder = export_encoder_ == QLatin1String("hardware")   ? ExportRequest::Encoder::Hardware
                                     : export_encoder_ == QLatin1String("software") ? ExportRequest::Encoder::Software
                                                                                    : ExportRequest::Encoder::Auto};
    export_total_ = request.frames;
    export_done_.store(0);
    export_progress_ = 0;
    exported_file_.clear();
    emit exportChanged();
    // Progress crosses threads through one atomic; the UI polls it, so the job never waits on
    // the UI thread (CLAUDE.md §13).
    export_timer_.start(); // connected in the constructor
    export_job_ = export_pool_.submit("export", [this, request = std::move(request), path](oma::JobContext& job) {
        ExportStats stats;
        auto done = export_timeline(request, [this, &job](std::int64_t k, std::int64_t) {
            export_done_.store(k);
            return !job.is_cancelled();
        }, &stats);
        const bool cancelled = !done && done.error().code() == oma::ErrorCode::Cancelled;
        const QString why = done || cancelled ? QString() : message(done.error());
        QMetaObject::invokeMethod(this, [this, path, why, cancelled, stats] {
            export_stats_ = stats;
            export_timer_.stop();
            export_progress_ = -1;
            if (cancelled) {
                setNotice(QStringLiteral("Export cancelled"));
            } else if (!why.isEmpty()) {
                setNotice(QStringLiteral("Cannot export %1: %2").arg(QFileInfo(path).fileName(), why));
            } else {
                exported_file_ = path;
                // Captions travel beside the movie as SubRip (ADR-0017).
                if (editor_ && !editor_->timeline().captions().empty()) {
                    exportCaptions(QUrl::fromLocalFile(QFileInfo(path).path() + QLatin1Char('/') +
                                                       QFileInfo(path).completeBaseName() + QStringLiteral(".srt")));
                }
                setNotice(QStringLiteral("Exported %1 · %2").arg(QFileInfo(path).fileName(), QString::fromStdString(stats.encoder)));
                // The user may have gone elsewhere during a long export (M7: notification).
                if (QGuiApplication::applicationState() != Qt::ApplicationActive && notifications_) {
                    notify_desktop(QStringLiteral("Export finished"), QFileInfo(path).fileName());
                }
            }
            emit exportChanged();
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

void Session::cancelExport() { export_job_.cancel(); }

void Session::openProject(const QUrl& url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty()) return;
    setNotice(QStringLiteral("Opening %1…").arg(QFileInfo(path).fileName()));
    const unsigned generation = ++generation_; // drops results of work for the session it replaces
    imports_.push_back(workers_.submit("open-project", [this, generation, path](oma::JobContext&) {
        auto doc = oma::project::load(std::filesystem::path(path.toStdString()));
        std::optional<oma::project::Document> loaded;
        QString why;
        if (doc) {
            loaded = std::move(*doc);
        } else {
            why = message(doc.error());
        }
        // The Timeline inside is copyable; a shared_ptr keeps the functor copyable for Qt.
        auto shared = std::make_shared<std::optional<oma::project::Document>>(std::move(loaded));
        QMetaObject::invokeMethod(this, [this, generation, path, why, shared] {
            if (generation != generation_) return;
            if (!*shared) {
                fail(QStringLiteral("Cannot open %1: %2").arg(QFileInfo(path).fileName(), why));
                return;
            }
            applyProject(std::move(**shared), path);
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

void Session::applyProject(oma::project::Document doc, const QString& path) {
    newProject();
    const bool restoring = !restoring_.isEmpty() && path == restoring_;
    if (restoring) {
        // A recovered autosave: the project it belonged to, with everything unsaved.
        project_path_ = restoring_origin_;
        if (!restoring_version_) autosave_file_ = restoring_; // a prior version is not consumed
        restoring_.clear();
        restoring_version_ = false;
    } else {
        project_path_ = path;
        rememberProject(path);
    }
    canvas_width_ = doc.canvas_width;
    canvas_height_ = doc.canvas_height;
    if (doc.timeline) {
        editor_.emplace(std::move(*doc.timeline));
        primary_ = doc.storyline;
        // The saved canvas, as state of the timeline (ADR-0010), not an edit to undo.
        (void)editor_->execute(tl::edit::set_canvas(canvas_width_, canvas_height_));
        editor_->clear_history();
    }
    for (oma::project::MediaRef& m : doc.media) {
        next_media_ = std::max(next_media_, m.info.id.value() + 1);
        const QString file = QString::fromStdString(m.path.string());
        importFile(file, false, std::move(m));
    }
    for (const oma::project::LutRef& l : doc.luts) {
        next_lut_ = std::max(next_lut_, l.info.id.value() + 1);
        loadLut(QString::fromStdString(l.path.string()), l.info.id);
    }
    // Saved as soon as everything it lists is back in the library.
    if (!restoring) {
        saved_ = {editor_.has_value(), editor_ ? editor_->revision() : 0, doc.media.size(), doc.luts.size(), library_revision_};
    }
    if (editor_) refreshSnapshot();
    emit sequenceChanged();
    emit selectionChanged();
    emit projectChanged();
    setFrame(0);
}

void Session::loadLut(const QString& path, tl::LutId id) {
    lut_paths_[id.value()] = path;
    const unsigned generation = generation_;
    imports_.push_back(workers_.submit("lut-load", [this, generation, path, id](oma::JobContext&) {
        QFile file(path);
        std::optional<oma::Result<oma::compositor::Lut3d>> parsed;
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.read(static_cast<qint64>(oma::compositor::kMaxCubeBytes) + 1);
            parsed = oma::compositor::parse_cube(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        }
        auto table = parsed && *parsed ? std::make_shared<const oma::compositor::Lut3d>(std::move(**parsed)) : nullptr;
        QMetaObject::invokeMethod(this, [this, generation, path, id, table = std::move(table)] {
            if (generation != generation_) return;
            if (!table) {
                // Clips keep referring to it; they show ungraded until the file is back.
                setNotice(QStringLiteral("Cannot load the LUT %1").arg(QFileInfo(path).fileName()));
                return;
            }
            auto tables = std::make_shared<LutTables>(*luts_);
            tables->emplace(id.value(), table);
            luts_ = std::move(tables);
            emit lutsChanged();
            requestFrame();
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

void Session::setClipLut(double id, double amount) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(id) || !std::isfinite(amount)) return;
    const tl::LutId lut(static_cast<std::uint64_t>(std::max(id, 0.0)));
    if (lut.valid() && editor_->timeline().find_lut(lut) == nullptr) return;
    tl::VideoProperties video = c->video;
    video.grade.lut = lut;
    video.grade.lut_amount = std::clamp(std::round(amount * 100.0) / 100.0, 0.0, 1.0);
    setSelectedVideo(video);
}

void Session::resetClipGrade() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    video.grade = {};
    setSelectedVideo(video);
}

void Session::setSelectedVideo(const tl::VideoProperties& video) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || video == c->video) return;
    const tl::ClipId id = c->id;
    run(tl::edit::set_video(id, video));
}

void Session::setClipColor(double exposure, double contrast, double saturation, double temperature) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    // Two decimals: slider noise does not make a new history entry.
    const auto snap = [](double v, double limit) {
        return std::isfinite(v) ? std::clamp(std::round(v * 100.0) / 100.0, -limit, limit) : 0.0;
    };
    tl::VideoProperties video = c->video;
    video.color = {.exposure = snap(exposure, 4.0),
                   .contrast = snap(contrast, 1.0),
                   .saturation = snap(saturation, 1.0),
                   .temperature = snap(temperature, 1.0)};
    setSelectedVideo(video);
}

QVariantList Session::lookEffects() {
    QVariantList out;
    for (const tl::EffectDefinition& d : tl::effect_definitions()) {
        if (d.stage != tl::EffectStage::Look) continue;
        out.push_back(QVariantMap{{"id", QString::fromUtf8(d.id.data(), static_cast<qsizetype>(d.id.size()))},
                                  {"name", QString::fromUtf8(d.name.data(), static_cast<qsizetype>(d.name.size()))}});
    }
    return out;
}

void Session::setClipEffect(const QString& definition, bool on) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    const std::string id = definition.toStdString();
    tl::VideoProperties video = c->video;
    if (on) {
        const tl::EffectDefinition* d = tl::find_effect_definition(id);
        if (d == nullptr) return;
        video.effects = tl::with_effect(std::move(video.effects), *d);
    } else {
        std::erase_if(video.effects, [&](const tl::Effect& e) { return e.definition == id; });
    }
    setSelectedVideo(video);
}

void Session::setClipEffectEnabled(const QString& definition, bool enabled) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    const std::string id = definition.toStdString();
    for (tl::Effect& e : video.effects) {
        if (e.definition == id) e.enabled = enabled;
    }
    setSelectedVideo(video);
}

void Session::setClipEffectParam(const QString& definition, const QString& name, double value) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const tl::EffectDefinition* d = tl::find_effect_definition(definition.toStdString());
    if (c == nullptr || d == nullptr || !std::isfinite(value)) return;
    const std::string param = name.toStdString();
    const auto known = std::ranges::find(d->params, param, &tl::ParamDefinition::name);
    if (known == d->params.end()) return;
    tl::VideoProperties video = c->video;
    video.effects = tl::with_effect(std::move(video.effects), *d);
    // Two decimals: slider noise does not make a new history entry.
    const double snapped = std::clamp(std::round(value * 100.0) / 100.0, known->min, known->max);
    for (tl::Effect& e : video.effects) {
        if (e.definition != d->id) continue;
        const auto p = std::ranges::find(e.params, param, &tl::EffectParam::name);
        if (p == e.params.end()) {
            e.params.push_back({.name = param, .value = snapped});
        } else {
            p->value = snapped;
        }
    }
    setSelectedVideo(video);
}

void Session::moveClipEffect(const QString& definition, int step) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    video.effects = tl::with_effect_moved(std::move(video.effects), definition.toStdString(), step);
    setSelectedVideo(video);
}

void Session::setTransition(double clip, int kind, double seconds) {
    if (!editor_ || !std::isfinite(seconds) || kind < -1 || kind > static_cast<int>(tl::TransitionKind::Wipe)) return;
    const tl::ClipId id(static_cast<std::uint64_t>(clip));
    const tl::Clip* c = editor_->timeline().find_clip(id);
    if (c == nullptr) return;
    std::optional<tl::Transition> transition;
    if (kind >= 0) {
        // Whole frames, at least two so each side gets one.
        const std::int64_t frames = std::max<std::int64_t>(2, std::llround(seconds * frameRate()));
        transition = tl::Transition{.kind = static_cast<tl::TransitionKind>(kind),
                                    .duration = editor_->timeline().at(frames * ticksPerFrame())};
    }
    if (transition == c->transition_in) return;
    if (run(tl::edit::set_transition(id, transition)) && transition &&
        !tl::transition_window(editor_->timeline(), *editor_->timeline().track_of(id),
                               static_cast<std::size_t>(c - editor_->timeline().track_of(id)->clips.data()))) {
        setNotice(QStringLiteral("The clips have no media to spare at this cut: it stays a plain cut"));
    }
}

void Session::addDissolveAtPlayhead() {
    if (!editor_) return;
    const tl::Track* track = editor_->timeline().find_track(primary_);
    if (track == nullptr) return;
    // The storyline cut closest to the playhead, between clips that touch.
    const tl::Clip* best = nullptr;
    for (std::size_t i = 1; i < track->clips.size(); ++i) {
        const tl::Clip& c = track->clips[i];
        if (track->clips[i - 1].end_ticks() != c.start_ticks()) continue;
        if (best == nullptr || std::llabs(c.start_ticks() - playhead_) < std::llabs(best->start_ticks() - playhead_)) {
            best = &c;
        }
    }
    if (best == nullptr) {
        setNotice(QStringLiteral("Transitions go between two touching clips on the storyline"));
        return;
    }
    setTransition(static_cast<double>(best->id.value()), static_cast<int>(tl::TransitionKind::Dissolve), 1.0);
}

void Session::setClipSharpness(double sharpness) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(sharpness)) return;
    if (std::round(sharpness * 100.0) == 0.0) {
        setClipEffect(QStringLiteral("oma.detail"), false);
    } else {
        setClipEffectParam(QStringLiteral("oma.detail"), QStringLiteral("amount"), sharpness);
    }
}

void Session::setClipFraming(int fit, double left, double top, double right, double bottom) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || fit < 0 || fit > static_cast<int>(tl::Fit::Native)) return;
    // Each edge keeps at least a tenth of the picture between them.
    const auto edge = [](double v) { return std::isfinite(v) ? std::clamp(std::round(v * 1000.0) / 1000.0, 0.0, 0.45) : 0.0; };
    tl::VideoProperties video = c->video;
    video.fit = static_cast<tl::Fit>(fit);
    video.crop = {.left = edge(left), .top = edge(top), .right = edge(right), .bottom = edge(bottom)};
    // A crop gesture's preview leaves the snapshot ahead of the model; the edit replaces it.
    const bool previewed = std::exchange(previewing_, false);
    setSelectedVideo(video);
    if (previewed) {
        refreshSnapshot();
        requestFrame();
    }
}

void Session::setClipOpacity(double opacity) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(opacity)) return;
    tl::VideoProperties video = c->video;
    video.opacity = static_cast<float>(std::clamp(std::round(opacity * 100.0) / 100.0, 0.0, 1.0));
    if (video.opacity != c->video.opacity) setSelectedVideo(video);
}

void Session::setClipTransform(double x, double y, double scale, double rotation) {
    if (auto video = transformedVideo(x, y, scale, rotation)) {
        // A gesture's preview leaves the snapshot ahead of the model; the edit replaces it.
        previewing_ = false;
        setSelectedVideo(*video);
        refreshSnapshot();
        requestFrame();
    }
}

void Session::previewClipTransform(double x, double y, double scale, double rotation) {
    if (auto video = transformedVideo(x, y, scale, rotation)) previewSelectedVideo(*video);
}

std::optional<Session::Framing> Session::framing(const tl::Crop& crop, double x, double y, double scale,
                                                double rotation) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const LibraryItem* source = c != nullptr ? item(c->media) : nullptr;
    // A title is drawn at the canvas size (ADR-0015), so the canvas is its source.
    const bool title = c != nullptr && c->title.has_value();
    if ((!title && (source == nullptr || source->width == 0)) || canvas_width_ == 0) return std::nullopt;
    const std::uint32_t source_width = title ? canvas_width_ : source->width;
    const std::uint32_t source_height = title ? canvas_height_ : source->height;
    oma::compositor::Layer layer;
    layer.fit = static_cast<oma::compositor::Fit>(c->video.fit);
    layer.crop = {.left = crop.left, .top = crop.top, .right = crop.right, .bottom = crop.bottom};
    layer.transform = {.offset_x = x, .offset_y = y, .scale_x = scale, .scale_y = scale, .rotation = rotation};
    const oma::compositor::SourceGeometry geometry{.width = source_width, .height = source_height};
    // Unclipped: the box may extend past the frame while it is dragged out.
    return Framing{.matrix = oma::compositor::source_to_output(layer, geometry, canvas_width_, canvas_height_),
                   .width = static_cast<double>(source_width),
                   .height = static_cast<double>(source_height)};
}

QVariantMap Session::layerBox(double x, double y, double scale) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return {};
    const auto f = framing(c->video.crop, x, y, scale);
    if (!f) return {};
    const tl::Crop& crop = c->video.crop;
    const auto a = f->matrix.apply(f->width * crop.left, f->height * crop.top);
    const auto b = f->matrix.apply(f->width * (1 - crop.right), f->height * (1 - crop.bottom));
    if (!std::isfinite(a[0]) || !std::isfinite(a[1]) || !std::isfinite(b[0]) || !std::isfinite(b[1])) return {};
    return {{QStringLiteral("x"), std::min(a[0], b[0]) / canvas_width_},
            {QStringLiteral("y"), std::min(a[1], b[1]) / canvas_height_},
            {QStringLiteral("w"), std::abs(b[0] - a[0]) / canvas_width_},
            {QStringLiteral("h"), std::abs(b[1] - a[1]) / canvas_height_}};
}

namespace {

tl::Crop crop_of(double left, double top, double right, double bottom) {
    const auto edge = [](double v) { return std::isfinite(v) ? std::clamp(v, 0.0, 0.45) : 0.0; };
    return {.left = edge(left), .top = edge(top), .right = edge(right), .bottom = edge(bottom)};
}

} // namespace

QVariantMap Session::cropBox(double left, double top, double right, double bottom) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return {};
    const tl::Crop crop = crop_of(left, top, right, bottom);
    const QVariantMap m = motion(); // the transform shown at the playhead, as the move/scale box uses
    const double x = m.value("posX").toDouble();
    const double y = m.value("posY").toDouble();
    const double scale = m.value("scale", 1.0).toDouble();
    // The box unrotated (the viewer turns it); the handles where the rotated edges really are.
    const auto f = framing(crop, x, y, scale);
    const auto turned = framing(crop, x, y, scale, m.value("rotation").toDouble());
    if (!f || !turned) return {};
    const double l = f->width * crop.left;
    const double r = f->width * (1 - crop.right);
    const double tp = f->height * crop.top;
    const double bt = f->height * (1 - crop.bottom);
    const auto point = [&](double sx, double sy) {
        const auto p = turned->matrix.apply(sx, sy);
        return QVariantMap{{QStringLiteral("x"), p[0] / canvas_width_}, {QStringLiteral("y"), p[1] / canvas_height_}};
    };
    const auto a = f->matrix.apply(l, tp);
    const auto b = f->matrix.apply(r, bt);
    if (!std::isfinite(a[0]) || !std::isfinite(a[1]) || !std::isfinite(b[0]) || !std::isfinite(b[1])) return {};
    return {{QStringLiteral("x"), std::min(a[0], b[0]) / canvas_width_},
            {QStringLiteral("y"), std::min(a[1], b[1]) / canvas_height_},
            {QStringLiteral("w"), std::abs(b[0] - a[0]) / canvas_width_},
            {QStringLiteral("h"), std::abs(b[1] - a[1]) / canvas_height_},
            {QStringLiteral("left"), point(l, (tp + bt) / 2)},
            {QStringLiteral("top"), point((l + r) / 2, tp)},
            {QStringLiteral("right"), point(r, (tp + bt) / 2)},
            {QStringLiteral("bottom"), point((l + r) / 2, bt)}};
}

double Session::cropEdgeAt(int edge, double x, double y, double left, double top, double right, double bottom) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const tl::Crop from = crop_of(left, top, right, bottom);
    const double current[] = {from.left, from.top, from.right, from.bottom};
    if (c == nullptr || edge < 0 || edge > 3 || !std::isfinite(x) || !std::isfinite(y)) return edge >= 0 && edge <= 3 ? current[edge] : 0.0;
    const QVariantMap m = motion();
    const auto f = framing(from, m.value("posX").toDouble(), m.value("posY").toDouble(), m.value("scale", 1.0).toDouble(),
                           m.value("rotation").toDouble());
    const auto inverse = f ? f->matrix.inverse() : std::nullopt;
    if (!inverse) return current[edge];
    const auto s = inverse->apply(x * canvas_width_, y * canvas_height_);
    const double fraction = edge == 0 ? s[0] / f->width
                          : edge == 1 ? s[1] / f->height
                          : edge == 2 ? 1 - s[0] / f->width
                                      : 1 - s[1] / f->height;
    // Same rounding and range as setClipFraming, so the preview shows what the edit keeps.
    return std::clamp(std::round(fraction * 1000.0) / 1000.0, 0.0, 0.45);
}

void Session::previewClipFraming(double left, double top, double right, double bottom) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    video.crop = crop_of(left, top, right, bottom);
    previewSelectedVideo(video);
}

void Session::previewSelectedVideo(const tl::VideoProperties& video) {
    // UX-07: the picture follows a drag without an undo entry per step. The snapshot the
    // viewer renders gets the change on a copy of the timeline; release commits one edit.
    tl::Editor scratch(editor_->timeline());
    if (scratch.execute(tl::edit::set_video(selected_clip_, video))) {
        snapshot_ = std::make_shared<const tl::Timeline>(scratch.timeline());
        previewing_ = true;
        requestFrame();
    }
}

std::optional<tl::VideoProperties> Session::transformedVideo(double x, double y, double scale, double rotation) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(scale) || !std::isfinite(rotation)) return std::nullopt;
    tl::VideoProperties video = c->video;
    const double s = std::clamp(std::round(scale * 100.0) / 100.0, 0.1, 4.0);
    const tl::Transform transform{.offset_x = std::round(x),
                                  .offset_y = std::round(y),
                                  .scale_x = s,
                                  .scale_y = s,
                                  .rotation = std::clamp(std::round(rotation * 10.0) / 10.0, -180.0, 180.0)};
    auto& keys = video.transform_keys;
    if (keys.empty()) {
        video.transform = transform;
    } else if (const auto at = keyTime(*c)) {
        const auto it = std::ranges::lower_bound(keys, *at, std::less{}, &tl::TransformKey::at);
        if (it != keys.end() && it->at == *at) {
            it->value = transform;
        } else if (keys.size() < tl::kMaxKeys) {
            keys.insert(it, tl::TransformKey{.at = *at, .value = transform});
        }
    }
    return video;
}

std::optional<oma::RationalTime> Session::keyTime(const tl::Clip& c) const {
    // A key at the clip's very end would sit past its last frame; the last frame is the limit.
    const std::int64_t ticks = std::clamp(playhead_, c.start_ticks(), c.end_ticks() - 1);
    auto t = tl::source_time(editor_->timeline(), c, ticks);
    return t ? std::optional(*t) : std::nullopt;
}

void Session::toggleTransformKey() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const auto at = c != nullptr ? keyTime(*c) : std::nullopt;
    if (!at) return;
    tl::VideoProperties video = c->video;
    auto& keys = video.transform_keys;
    const auto it = std::ranges::lower_bound(keys, *at, std::less{}, &tl::TransformKey::at);
    if (it != keys.end() && it->at == *at) {
        if (keys.size() == 1) video.transform = it->value; // the picture stays where it was
        keys.erase(it);
    } else if (keys.size() < tl::kMaxKeys) {
        keys.insert(it, tl::TransformKey{.at = *at, .value = tl::transform_at(c->video, *at)});
    }
    setSelectedVideo(video);
}

void Session::resetClipFraming() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    tl::VideoProperties video = c->video;
    video.fit = tl::Fit::Fit;
    video.crop = {};
    video.transform = {};
    video.transform_keys.clear();
    setSelectedVideo(video);
}

void Session::kenBurns() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return;
    const tl::Timeline& t = editor_->timeline();
    auto first = tl::source_time(t, *c, c->start_ticks());
    auto last = tl::source_time(t, *c, c->end_ticks() - 1);
    if (!first || !last || !(*first < *last)) return;
    tl::VideoProperties video = c->video;
    const tl::Transform from = tl::transform_at(c->video, *first);
    tl::Transform to = from;
    to.scale_x = std::clamp(from.scale_x * 1.2, -4.0, 4.0);
    to.scale_y = std::clamp(from.scale_y * 1.2, -4.0, 4.0);
    video.transform_keys = {tl::TransformKey{.at = *first, .value = from, .interpolation = tl::Interpolation::Ease},
                            tl::TransformKey{.at = *last, .value = to}};
    setSelectedVideo(video);
}

QVariantMap Session::motion() const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr) return {};
    const auto at = keyTime(*c);
    const tl::Transform tr = at ? tl::transform_at(c->video, *at) : c->video.transform;
    const auto& keys = c->video.transform_keys;
    const bool here = at && std::ranges::binary_search(keys, *at, std::less{}, &tl::TransformKey::at);
    return {{QStringLiteral("posX"), tr.offset_x},
            {QStringLiteral("posY"), tr.offset_y},
            {QStringLiteral("scale"), tr.scale_x},
            {QStringLiteral("rotation"), tr.rotation},
            {QStringLiteral("keys"), static_cast<int>(keys.size())},
            {QStringLiteral("keyHere"), here}};
}

namespace {

// Linear premultiplied RGBA -> an sRGB-encoded 8-bit image, for previews only.
QImage to_srgb_image(const oma::compositor::RgbaImage& image) {
    QImage out(static_cast<int>(image.width), static_cast<int>(image.height), QImage::Format_RGBA8888);
    const auto encode = [](float linear) {
        const double x = std::clamp(static_cast<double>(linear), 0.0, 1.0);
        const double v = x <= 0.0031308 ? 12.92 * x : (1.055 * std::pow(x, 1.0 / 2.4)) - 0.055;
        return static_cast<int>(std::lround(v * 255.0));
    };
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto p = image.at(x, y);
            const float a = p[3] > 0.0F ? p[3] : 1.0F;
            out.setPixelColor(static_cast<int>(x), static_cast<int>(y),
                              QColor(encode(p[0] / a), encode(p[1] / a), encode(p[2] / a),
                                     static_cast<int>(std::lround(std::clamp(p[3], 0.0F, 1.0F) * 255.0F))));
        }
    }
    return out;
}

} // namespace

void Session::requestFilterPreviews() {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const LibraryItem* source = c != nullptr ? item(c->media) : nullptr;
    if (c == nullptr || source == nullptr || !source->media.has_video) return;
    const unsigned request = ++previews_;
    const unsigned generation = generation_;
    const std::string path = source->path.toStdString();
    const oma::RationalTime at = c->source_in;
    const tl::VideoProperties video = c->video;
    constexpr std::uint32_t width = 160; // a constant: lambdas use it without capturing it
    const std::uint32_t height = std::max<std::uint32_t>(2, width * canvas_height_ / std::max<std::uint32_t>(1, canvas_width_));
    const QString dir = thumbnails_.path();
    previews_job_.cancel(); // an older request still queued would only be thrown away
    previews_job_ = workers_.submit("filter-previews", [this, request, generation, path, at, video, height, dir](oma::JobContext& job) {
        auto picture = frames_.picture_at(path, at);
        QVariantList urls;
        if (picture) {
            oma::compositor::LayerInput input{.frame = picture->frame.get(),
                                              .color = picture->color,
                                              .rotation = picture->rotation,
                                              .sample_aspect = picture->sample_aspect};
            const std::array<oma::compositor::LayerInput, 1> inputs{input};
            const auto looks = std::ranges::to<std::vector>(
                tl::effect_definitions() | std::views::filter([](const tl::EffectDefinition& d) { return d.stage == tl::EffectStage::Look; }));
            for (std::size_t kind = 0; kind < looks.size() && !job.is_cancelled(); ++kind) {
                oma::compositor::RenderGraph graph;
                graph.width = width;
                graph.height = height;
                oma::compositor::Layer layer;
                layer.fit = static_cast<oma::compositor::Fit>(video.fit);
                layer.crop = {.left = video.crop.left, .top = video.crop.top, .right = video.crop.right, .bottom = video.crop.bottom};
                layer.color = {.exposure = video.color.exposure,
                               .contrast = video.color.contrast,
                               .saturation = video.color.saturation,
                               .temperature = video.color.temperature};
                layer.looks.push_back({.kind = static_cast<oma::compositor::FilterKind>(looks[kind].look), .amount = 1.0});
                graph.layers.push_back(layer);
                // CPU reference: tiny images, and no GPU work leaves the render thread (ADR-0005).
                auto rendered = oma::compositor::CpuCompositor{}.render(graph, inputs);
                const QString file = QStringLiteral("%1/filter-%2-%3.png").arg(dir).arg(request).arg(kind);
                if (rendered && to_srgb_image(*rendered).save(file)) {
                    urls.push_back(QUrl::fromLocalFile(file).toString());
                } else {
                    urls.push_back(QString());
                }
            }
        }
        QMetaObject::invokeMethod(this, [this, request, generation, urls = std::move(urls)] {
            if (generation != generation_ || request != previews_) return;
            filter_previews_ = urls;
            emit filterPreviewsChanged();
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

void Session::undo() {
    if (!editor_ || !editor_->can_undo()) return;
    pause();
    if (auto r = editor_->undo(); !r) setNotice(message(r.error()));
    afterEdit();
}

void Session::redo() {
    if (!editor_ || !editor_->can_redo()) return;
    pause();
    if (auto r = editor_->redo(); !r) setNotice(message(r.error()));
    afterEdit();
}

// ------------------------------------------------------------------- read-out for QML

QVariantList Session::media() const {
    QVariantList list;
    for (const LibraryItem& i : library_) {
        list.push_back(QVariantMap{{"id", static_cast<double>(i.media.id.value())},
                                   {"audioOnly", i.audio_only},
                                   {"name", i.name},
                                   {"duration", i.seconds},
                                   {"thumbnail", i.thumbnail},
                                   {"missing", i.missing},
                                   // The marked range Add/Insert/Overwrite will use (seconds; -1 unset).
                                   {"markIn", i.mark_in ? static_cast<double>(*i.mark_in) / frameRate() : -1.0},
                                   {"markOut", i.mark_out ? static_cast<double>(*i.mark_out) / frameRate() : -1.0}});
    }
    return list;
}

QVariantMap Session::clipMap(const tl::Clip& c, const tl::Track& track, std::size_t index) const {
    const LibraryItem* source = item(c.media);
    // The transition into this clip as it plays: its kind (-1 none, or not playable here) and
    // the span it covers around the clip's start, in seconds (display only).
    const auto window = tl::transition_window(editor_->timeline(), track, index);
    const tl::Timeline& t = editor_->timeline();
    const bool joined = index > 0 && track.clips[index - 1].end_ticks() == c.start_ticks();
    return QVariantMap{{"id", static_cast<double>(c.id.value())},
                       {"joined", joined},
                       {"connected", c.anchor.has_value()},
                       {"primary", c.anchor ? static_cast<double>(c.anchor->primary.value()) : -1.0},
                       // Anything changed from a plain clip: the timeline marks it.
                       {"adjusted", c.video != tl::VideoProperties{} || c.audio != tl::AudioProperties{}},
                       {"timing", timingLabel(c.time_map)},
                       {"transitionKind", window ? static_cast<int>(window->kind) : -1},
                       {"transitionSet", c.transition_in.has_value()},
                       {"transitionSpan", window ? t.at(2 * window->half).seconds_approx() : 0.0},
                       {"media", static_cast<double>(c.media.value())},
                       {"hasAudio", source != nullptr && source->media.has_audio && !c.audio_detached},
                       {"audioDetached", c.audio_detached},
                       {"sourceIn", c.source_in.seconds_approx()},
                       {"speed", c.time_map.speed().to_double_approx()},
                       {"gain", c.audio.muted ? 0.0 : static_cast<double>(c.audio.gain)},
                       {"start", c.start.seconds_approx()},
                       {"duration", c.duration.seconds_approx()},
                       {"name", source != nullptr ? source->name
                                : c.title ? QString::fromStdString(c.title->text).section(QLatin1Char('\n'), 0, 0)
                                          : QString()},
                       {"isTitle", c.title.has_value()},
                       {"thumbnail", source != nullptr ? source->thumbnail : QString()},
                       {"fadeIn", c.audio.fade_in.seconds_approx()},
                       {"fadeOut", c.audio.fade_out.seconds_approx()},
                       {"audioAdjusted", c.audio.muted || c.audio.gain != 1.0F || c.audio.fade_in.value() != 0 ||
                                             c.audio.fade_out.value() != 0 || c.audio.eq != tl::Equalizer{} ||
                                             c.audio.noise.amount > 0.0F}};
}

QVariantList Session::clips() const {
    // Built once per change: QML reads this from several bindings after every edit (M6 long-form
    // audit), and each build is O(clips). Invalidated by sequenceChanged and libraryChanged.
    if (clips_cache_) return *clips_cache_;
    QVariantList list;
    if (!editor_) return list;
    const tl::Track* track = editor_->timeline().find_track(primary_);
    if (track == nullptr) return list;
    list.reserve(static_cast<qsizetype>(track->clips.size()));
    for (std::size_t i = 0; i < track->clips.size(); ++i) list.push_back(clipMap(track->clips[i], *track, i));
    clips_cache_ = list;
    return list;
}

void Session::setStorylineView(double from, double to) {
    if (!std::isfinite(from) || std::isnan(to) || (from == view_from_ && to == view_to_)) return;
    view_from_ = from;
    view_to_ = to;
    syncStoryline();
}

// O(log clips + clips in view): only the clips the view can show get rows (and delegates).
void Session::syncStoryline() {
    constexpr qsizetype kMaxDelegates = 400;
    QVariantList rows;
    const tl::Track* track = editor_ ? editor_->timeline().find_track(primary_) : nullptr;
    bool compact = false;
    if (track != nullptr && !track->clips.empty()) {
        const std::int64_t from = std::max<std::int64_t>(0, std::llround(view_from_ * frameRate())) * ticksPerFrame();
        const std::int64_t to = std::isfinite(view_to_)
                                    ? std::llround(view_to_ * frameRate()) * ticksPerFrame()
                                    : std::numeric_limits<std::int64_t>::max();
        auto it = std::ranges::upper_bound(track->clips, from, {}, &tl::Clip::end_ticks);
        for (; it != track->clips.end() && it->start_ticks() <= to; ++it) {
            if (rows.size() >= kMaxDelegates) {
                compact = true;
                break;
            }
            rows.push_back(clipMap(*it, *track, static_cast<std::size_t>(it - track->clips.begin())));
        }
    }
    if (compact) rows.clear();
    storyline_model_.sync(rows);
    if (compact != storyline_compact_) {
        storyline_compact_ = compact;
        emit storylineViewChanged();
    }
}

void Session::selectClipAt(double seconds) {
    if (!editor_ || !std::isfinite(seconds)) return;
    if (const tl::Clip* c = editor_->timeline().clip_at(primary_, ticksAt(seconds))) {
        selectClip(static_cast<double>(c->id.value()));
        seek(seconds);
    }
}

QList<double> Session::clipSpans() const {
    QList<double> out;
    const tl::Track* track = editor_ ? editor_->timeline().find_track(primary_) : nullptr;
    if (track == nullptr) return out;
    out.reserve(static_cast<qsizetype>(track->clips.size() * 2));
    for (const tl::Clip& c : track->clips) {
        out.push_back(c.start.seconds_approx());
        out.push_back(c.duration.seconds_approx());
    }
    return out;
}

QList<double> Session::selectedSpan() const { return clipSpan(static_cast<double>(selected_clip_.value())); }

QList<double> Session::clipSpan(double id) const {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(tl::ClipId(static_cast<std::uint64_t>(id))) : nullptr;
    if (c == nullptr) return {};
    return {c->start.seconds_approx(), c->duration.seconds_approx()};
}

QVariantList Session::videoTracks() const {
    QVariantList out;
    for (const tl::TrackId id : lanes(tl::TrackKind::Video)) {
        const tl::Track* track = editor_->timeline().find_track(id);
        QVariantList clips;
        for (std::size_t i = 0; i < track->clips.size(); ++i) clips.push_back(clipMap(track->clips[i], *track, i));
        out.push_back(QVariantMap{{"id", static_cast<double>(id.value())},
                                  {"name", QString::fromStdString(track->name)},
                                  {"clips", clips}});
    }
    return out;
}

void Session::connectSelected() {
    if (selected_media_ < 0 || selected_media_ >= static_cast<int>(library_.size())) {
        setNotice(QStringLiteral("Select something in the library to connect"));
        return;
    }
    const LibraryItem& source = library_[static_cast<std::size_t>(selected_media_)];
    if (!ensureSequence(source)) return;
    const auto clip = sourceFor(source);
    if (!clip) return;
    const tl::ClipId id = editor_->new_clip_id();
    if (placeOnLane(source.audio_only ? tl::TrackKind::Audio : tl::TrackKind::Video, 1, id, *clip, playhead_, {})) {
        selected_clip_ = id;
        emit selectionChanged();
    }
}

void Session::addTitle() {
    if (!editor_) {
        setNotice(QStringLiteral("Add a video first: a title goes over the storyline"));
        return;
    }
    const tl::Timeline& t = editor_->timeline();
    const auto seconds = oma::RationalTime::make(3, oma::Rational::literal(1, 1));
    const auto ticks = seconds ? t.to_ticks(*seconds) : oma::Result<std::int64_t>(std::unexpected(seconds.error()));
    if (!ticks) return;
    tl::Title title;
    title.text = "Title";
    const tl::edit::ClipSource clip{.media = tl::MediaId{},
                                    .source_in = t.at(0),
                                    .duration = t.at(*ticks),
                                    .time_map = {},
                                    .video = {},
                                    .audio = {},
                                    .title = title};
    const tl::ClipId id = editor_->new_clip_id();
    if (placeOnLane(tl::TrackKind::Video, 1, id, clip, playhead_, {}, "Add Title")) {
        selected_clip_ = id;
        emit selectionChanged();
    }
}

void Session::setClipTitle(const QString& text, double size, const QString& color, int placement) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    if (c == nullptr || !c->title) return;
    tl::Title title = *c->title;
    // Bounded like the format (ADR-0015): long text is cut at a character boundary.
    QByteArray utf8 = text.toUtf8();
    while (utf8.size() > static_cast<qsizetype>(tl::kMaxTitleBytes)) {
        utf8 = QString::fromUtf8(utf8).chopped(1).toUtf8();
    }
    title.text = utf8.toStdString();
    if (std::isfinite(size)) title.size = std::clamp(std::round(size * 1000.0) / 1000.0, 0.02, 0.5);
    if (const QColor rgb(color); rgb.isValid()) {
        title.color = {static_cast<float>(rgb.redF()), static_cast<float>(rgb.greenF()), static_cast<float>(rgb.blueF()),
                       title.color[3]};
    }
    if (placement >= 0 && placement <= static_cast<int>(tl::TitlePlacement::Top)) {
        title.placement = static_cast<tl::TitlePlacement>(placement);
    }
    if (title != *c->title) run(tl::edit::set_title(c->id, title));
}

void Session::connectSelectedClip() {
    if (!editor_) return;
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* c = t.find_clip(selected_clip_);
    const tl::Clip* primary = c != nullptr ? t.clip_at(primary_, c->start_ticks()) : nullptr;
    if (c == nullptr || primary == nullptr || t.track_of(c->id)->id == primary_) {
        setNotice(QStringLiteral("Place the clip's start over a storyline clip to connect it"));
        return;
    }
    run(tl::edit::connect(c->id, primary->id));
}

void Session::disconnectSelectedClip() {
    if (editor_ && editor_->timeline().find_clip(selected_clip_) != nullptr) run(tl::edit::disconnect(selected_clip_));
}

QVariantList Session::audioTracks() const {
    QVariantList lanes;
    for (const tl::TrackId id : audioLanes()) {
        const tl::Track* track = editor_->timeline().find_track(id);
        QVariantList clips;
        for (std::size_t i = 0; i < track->clips.size(); ++i) clips.push_back(clipMap(track->clips[i], *track, i));
        lanes.push_back(QVariantMap{{"id", static_cast<double>(id.value())},
                                    {"name", QString::fromStdString(track->name)},
                                    {"clips", clips}});
    }
    return lanes;
}

QVariantMap Session::info() const {
    const LibraryItem* source = nullptr;
    const tl::Clip* selected = editor_ && selected_clip_.valid() ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const tl::Title* title = selected != nullptr && selected->title ? &*selected->title : nullptr;
    if (selected != nullptr && title == nullptr) source = item(selected->media);
    if (source == nullptr && title == nullptr && selected_media_ >= 0 && selected_media_ < static_cast<int>(library_.size()))
        source = &library_[static_cast<std::size_t>(selected_media_)];
    if (source == nullptr && title == nullptr) return {};
    QVariantMap out = source != nullptr ? source->details : QVariantMap{};
    if (title != nullptr) {
        // A title has no library item (ADR-0015): its text names it.
        out.insert("name", QString::fromStdString(title->text).section(QLatin1Char('\n'), 0, 0));
        out.insert("duration", selected->duration.seconds_approx());
        out.insert("hasAudio", false);
        out.insert("codec", QStringLiteral("Title"));
        out.insert("decodePath", QStringLiteral("drawn by OmaMovie"));
        out.insert("isTitle", true);
        out.insert("titleText", QString::fromStdString(title->text));
        out.insert("titleSize", title->size);
        out.insert("titleColor", QColor::fromRgbF(title->color[0], title->color[1], title->color[2]).name());
        out.insert("titlePlacement", static_cast<int>(title->placement));
    } else {
        out.insert("name", source->name);
        out.insert("duration", source->seconds);
        out.insert("hasAudio", source->media.has_audio);
    }
    if (editor_ && selected_clip_.valid()) {
        if (const tl::Clip* c = editor_->timeline().find_clip(selected_clip_)) {
            out.insert("clip", true);
            // Detached sound is adjusted on its own clip.
            if (c->audio_detached) out.insert("hasAudio", false);
            out.insert("onStoryline", editor_->timeline().track_of(c->id)->id == primary_);
            out.insert("connected", c->anchor.has_value());
            out.insert("primary", c->anchor ? static_cast<double>(c->anchor->primary.value()) : -1.0);
            out.insert("canDetach", source != nullptr && source->media.has_audio && !c->audio_detached &&
                                        editor_->timeline().track_of(c->id)->id == primary_);
            out.insert("clipDuration", c->duration.seconds_approx());
            out.insert("gain", static_cast<double>(c->audio.gain));
            out.insert("fadeIn", c->audio.fade_in.seconds_approx());
            out.insert("fadeOut", c->audio.fade_out.seconds_approx());
            out.insert("muted", c->audio.muted);
            out.insert("eqLow", static_cast<double>(c->audio.eq.low_db));
            out.insert("eqMid", static_cast<double>(c->audio.eq.mid_db));
            out.insert("eqHigh", static_cast<double>(c->audio.eq.high_db));
            out.insert("noise", static_cast<double>(c->audio.noise.amount));
            const tl::Track* track = editor_->timeline().track_of(c->id);
            out.insert("hasVideo", track != nullptr && track->kind == tl::TrackKind::Video);
            const tl::VideoProperties& v = c->video;
            out.insert("exposure", v.color.exposure);
            out.insert("contrast", v.color.contrast);
            out.insert("saturation", v.color.saturation);
            out.insert("temperature", v.color.temperature);
            QVariantList effects; // the stack except detail, which has its own slider
            double sharpness = 0.0;
            for (const tl::Effect& e : v.effects) {
                const tl::EffectDefinition* d = tl::find_effect_definition(e.definition);
                if (d != nullptr && d->stage == tl::EffectStage::Detail) {
                    sharpness = tl::effect_param(e, "amount");
                    continue;
                }
                effects.push_back(QVariantMap{
                    {"id", QString::fromStdString(e.definition)},
                    {"name", d != nullptr ? QString::fromUtf8(d->name.data(), static_cast<qsizetype>(d->name.size()))
                                          : QString::fromStdString(e.definition)},
                    {"known", d != nullptr},
                    {"enabled", e.enabled},
                    {"amount", tl::effect_param(e, "amount")}});
            }
            out.insert("effects", effects);
            out.insert("sharpness", sharpness);
            out.insert("fit", static_cast<int>(v.fit));
            out.insert("cropLeft", v.crop.left);
            out.insert("cropTop", v.crop.top);
            out.insert("cropRight", v.crop.right);
            out.insert("cropBottom", v.crop.bottom);
            out.insert("colorAdjusted", v.color != tl::ColorAdjust{} || v.grade != tl::ColorGrade{});
            out.insert("graded", v.grade != tl::ColorGrade{});
            out.insert("wheels", wheels_of(v.grade.cdl));
            QVariantMap curves;
            const std::array<const std::vector<tl::CurvePoint>*, 4> all{&v.grade.curves.master, &v.grade.curves.red,
                                                                       &v.grade.curves.green, &v.grade.curves.blue};
            for (std::size_t ch = 0; ch < all.size(); ++ch) {
                QVariantList points;
                for (const tl::CurvePoint& p : *all[ch]) points.push_back(QVariantMap{{"x", p.x}, {"y", p.y}});
                curves.insert(QString::number(ch), points);
            }
            out.insert("curves", curves);
            out.insert("lut", static_cast<double>(v.grade.lut.value()));
            out.insert("lutAmount", v.grade.lut_amount);
            out.insert("opacity", static_cast<double>(v.opacity));
            out.insert("framingAdjusted", v.fit != tl::Fit::Fit || v.crop != tl::Crop{} || v.transform != tl::Transform{} ||
                                              !v.transform_keys.empty() || v.opacity != 1.0F);
            out.insert("filtered", !v.effects.empty());
        }
    }
    return out;
}

bool Session::hasMedia() const {
    return editor_ && editor_->timeline().duration().value() > 0;
}

double Session::duration() const {
    return editor_ ? editor_->timeline().duration().seconds_approx() : 0;
}

double Session::frameRate() const {
    return editor_ ? editor_->timeline().frame_rate().fps().to_double_approx() : 30;
}

QString Session::undoText() const {
    return editor_ ? QString::fromUtf8(editor_->undo_name()) : QString();
}

QString Session::redoText() const {
    return editor_ ? QString::fromUtf8(editor_->redo_name()) : QString();
}

double Session::position() const {
    return editor_ ? editor_->timeline().at(playhead_).seconds_approx() : 0;
}

// ------------------------------------------------------------------- playhead

std::int64_t Session::ticksPerFrame() const {
    const tl::Timeline& t = editor_->timeline();
    return t.to_ticks(t.frame_rate().frame_to_time(1)).value_or(1);
}

std::int64_t Session::frame() const {
    return editor_ ? playhead_ / ticksPerFrame() : 0;
}

// The last frame that shows something; the end of the sequence is exclusive.
std::int64_t Session::lastFrame() const {
    if (!editor_) return 0;
    const std::int64_t end = editor_->timeline().duration().value();
    return end > 0 ? (end - 1) / ticksPerFrame() : 0;
}

void Session::setFrame(std::int64_t frame) {
    if (!editor_) return;
    if (source_index_ >= 0) { // the source viewer has its own playhead
        source_frame_ = std::clamp<std::int64_t>(frame, 0, std::max<std::int64_t>(0, source_frames_ - 1));
        emit sourceChanged();
        requestFrame();
        return;
    }
    playhead_ = std::clamp<std::int64_t>(frame, 0, lastFrame()) * ticksPerFrame();
    emit positionChanged();
    requestFrame();
}

void Session::seek(double seconds) {
    if (!editor_ || !std::isfinite(seconds)) return;
    closeSource(); // a sequence time (the timeline, its menus) means the sequence again
    // Display seconds to the nearest frame of the sequence grid.
    setFrame(std::llround(seconds * frameRate()));
    if (playing()) startPlayback(); // restart the clock, audio and decode-ahead from here
}

void Session::stepFrames(int frames) {
    pause();
    setFrame((source_index_ >= 0 ? source_frame_ : frame()) + frames);
}

// ------------------------------------------------------------------- captions

QVariantList Session::captions() const {
    QVariantList out;
    if (!editor_) return out;
    for (const tl::Caption& c : editor_->timeline().captions()) {
        out.push_back(QVariantMap{{"id", static_cast<double>(c.id.value())},
                                  {"start", c.start.seconds_approx()},
                                  {"duration", c.duration.seconds_approx()},
                                  {"text", QString::fromStdString(c.text)}});
    }
    return out;
}

namespace {

// The caption covering sequence tick `at` (captions are sorted and never overlap): O(log n).
const tl::Caption* caption_covering(const tl::Timeline& t, std::int64_t at) {
    const auto list = t.captions();
    auto it = std::ranges::upper_bound(list, at, {}, [](const tl::Caption& c) { return c.start.value(); });
    if (it == list.begin()) return nullptr;
    --it;
    return at < it->start.value() + it->duration.value() ? &*it : nullptr;
}

} // namespace

QString Session::captionAt(double seconds) const {
    if (!editor_ || !std::isfinite(seconds)) return {};
    const tl::Caption* c = caption_covering(editor_->timeline(), ticksAt(seconds));
    return c != nullptr ? QString::fromStdString(c->text) : QString();
}

double Session::captionIdAt(double seconds) const {
    if (!editor_ || !std::isfinite(seconds)) return 0;
    const tl::Caption* c = caption_covering(editor_->timeline(), ticksAt(seconds));
    return c != nullptr ? static_cast<double>(c->id.value()) : 0;
}

double Session::addCaption() {
    if (!editor_) return 0;
    const tl::Timeline& t = editor_->timeline();
    if (caption_covering(t, playhead_) != nullptr) {
        setNotice(QStringLiteral("A caption is already here: double-click it to edit"));
        return 0;
    }
    // Three seconds, or up to the next caption.
    std::int64_t end = playhead_ + (3 * static_cast<std::int64_t>(std::llround(frameRate())) * ticksPerFrame());
    const auto list = t.captions();
    const auto next = std::ranges::upper_bound(list, playhead_, {}, [](const tl::Caption& c) { return c.start.value(); });
    if (next != list.end()) end = std::min(end, next->start.value());
    if (end <= playhead_) return 0;
    const tl::CaptionId id = editor_->new_caption_id();
    if (!run(tl::edit::add_caption({.id = id, .start = t.at(playhead_), .duration = t.at(end - playhead_), .text = "Caption"}))) {
        return 0;
    }
    return static_cast<double>(id.value());
}

void Session::setCaptionText(double id, const QString& text) {
    if (!editor_) return;
    const auto list = editor_->timeline().captions();
    const auto it = std::ranges::find(list, tl::CaptionId(static_cast<std::uint64_t>(id)), &tl::Caption::id);
    const std::string utf8 = text.toStdString();
    if (it == list.end() || it->text == utf8) return;
    if (utf8.size() > tl::kMaxCaptionBytes) {
        setNotice(QStringLiteral("A caption holds at most %1 bytes of text").arg(tl::kMaxCaptionBytes));
        return;
    }
    tl::Caption changed = *it;
    changed.text = utf8;
    run(tl::edit::set_caption(std::move(changed)));
}

void Session::removeCaption(double id) {
    if (editor_) run(tl::edit::remove_caption(tl::CaptionId(static_cast<std::uint64_t>(id))));
}

void Session::importCaptions(const QUrl& url) {
    if (!editor_) return;
    QFile file(url.toLocalFile());
    if (file.size() > static_cast<qint64>(oma::project::kMaxSubtitleBytes) || !file.open(QIODevice::ReadOnly)) {
        setNotice(QStringLiteral("Cannot read %1").arg(QFileInfo(file.fileName()).fileName()));
        return;
    }
    const QByteArray bytes = file.readAll();
    auto cues = oma::project::parse_subtitles(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    if (!cues) {
        setNotice(QStringLiteral("%1: %2 (%3)").arg(QFileInfo(file.fileName()).fileName(),
                                                   QString::fromStdString(cues.error().message()),
                                                   QString::fromStdString(cues.error().context())));
        return;
    }
    // Onto the frame grid: starts round down, ends up, then each end stops at the next start.
    const tl::Timeline& t = editor_->timeline();
    const std::int64_t tpf = ticksPerFrame();
    std::vector<tl::Caption> captions;
    for (const oma::project::Cue& cue : *cues) {
        const auto from = oma::rescale(cue.start_ms, oma::Rational::literal(1, 1000), t.timebase(), oma::Rounding::Floor);
        const auto to = oma::rescale(cue.end_ms, oma::Rational::literal(1, 1000), t.timebase(), oma::Rounding::Ceil);
        if (!from || !to) continue;
        const std::int64_t start = *from / tpf * tpf;
        const std::int64_t end = (*to + tpf - 1) / tpf * tpf;
        if (!captions.empty()) {
            tl::Caption& last = captions.back();
            const std::int64_t last_end = std::min(last.start.value() + last.duration.value(), start);
            last.duration = t.at(last_end - last.start.value());
        }
        captions.push_back({.id = editor_->new_caption_id(), .start = t.at(start), .duration = t.at(end - start), .text = cue.text});
    }
    std::erase_if(captions, [](const tl::Caption& c) { return c.duration.value() <= 0; });
    if (run(tl::edit::replace_captions(std::move(captions)))) {
        setNotice(QStringLiteral("%1 captions imported").arg(editor_->timeline().captions().size()));
    }
}

void Session::exportCaptions(const QUrl& url) {
    if (!editor_) return;
    const QString path = url.toLocalFile();
    std::vector<oma::project::Cue> cues;
    for (const tl::Caption& c : editor_->timeline().captions()) {
        const auto ms = [](const oma::RationalTime& time) {
            return oma::rescale(time.value(), time.timebase(), oma::Rational::literal(1, 1000), oma::Rounding::Nearest).value_or(0);
        };
        cues.push_back({.start_ms = ms(c.start), .end_ms = ms(c.start) + std::max<std::int64_t>(1, ms(c.duration)), .text = c.text});
    }
    const std::string text = path.endsWith(QStringLiteral(".vtt"), Qt::CaseInsensitive) ? oma::project::write_vtt(cues)
                                                                                         : oma::project::write_srt(cues);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()) ||
        !out.commit()) {
        setNotice(QStringLiteral("Cannot write %1").arg(QFileInfo(path).fileName()));
        return;
    }
    setNotice(QStringLiteral("%1 captions saved to %2").arg(cues.size()).arg(QFileInfo(path).fileName()));
}

void Session::setCanvasAspect(int w, int h) {
    if (!editor_ || w <= 0 || h <= 0) return;
    // Same short side, new proportion, even sizes: 1920x1080 becomes 1080x1920 at 9:16.
    const std::uint32_t short_side = std::min(canvas_width_, canvas_height_);
    const auto even = [](double v) { return static_cast<std::uint32_t>(std::lround(v / 2.0)) * 2U; };
    const std::uint32_t width = w >= h ? even(static_cast<double>(short_side) * w / h) : short_side;
    const std::uint32_t height = w >= h ? short_side : even(static_cast<double>(short_side) * h / w);
    if (width == canvas_width_ && height == canvas_height_) return;
    if (run(tl::edit::set_canvas(width, height))) {
        setNotice(QStringLiteral("Canvas %1 × %2").arg(width).arg(height));
    }
}

void Session::openSource(int index) {
    if (index < 0 || index >= static_cast<int>(library_.size())) return;
    LibraryItem& item = library_[static_cast<std::size_t>(index)];
    if (item.missing) {
        setNotice(QStringLiteral("%1 is missing: locate it first").arg(item.name));
        return;
    }
    if (!ensureSequence(item)) return;
    pause();
    // The item as a one-clip timeline at the sequence's rate, ignoring its marks: the viewer's
    // frame builder, decoders and compositor then show it exactly as the sequence would.
    const auto marks = std::pair(std::exchange(item.mark_in, std::nullopt), std::exchange(item.mark_out, std::nullopt));
    const auto whole = sourceFor(item);
    item.mark_in = marks.first;
    item.mark_out = marks.second;
    const tl::Timeline& seq = editor_->timeline();
    auto created = tl::Timeline::create(seq.frame_rate(), kSequenceAudioRate);
    if (!whole || !created) return;
    tl::Editor ed(std::move(*created));
    const tl::TrackId track = ed.new_track_id();
    // Sound goes on an audio track: the viewer then shows the background while marks still work.
    const auto kind = item.audio_only ? tl::TrackKind::Audio : tl::TrackKind::Video;
    if (!ed.add_media(item.media) || !ed.execute(tl::edit::add_track(track, kind, "Source")) ||
        !ed.execute(tl::edit::append(track, ed.new_clip_id(), *whole))) {
        setNotice(QStringLiteral("Cannot show %1 in the source viewer").arg(item.name));
        return;
    }
    source_timeline_ = std::make_shared<const tl::Timeline>(ed.timeline());
    source_frames_ = source_timeline_->to_ticks(source_timeline_->duration()).value_or(0) /
                     std::max<std::int64_t>(1, source_timeline_->to_ticks(seq.frame_rate().frame_to_time(1)).value_or(1));
    source_index_ = index;
    selected_media_ = index;
    source_frame_ = item.mark_in.value_or(0);
    emit selectionChanged();
    emit sourceChanged();
    requestFrame();
}

void Session::seekSource(double seconds) {
    if (source_index_ < 0 || !std::isfinite(seconds)) return;
    setFrame(std::llround(seconds * frameRate())); // routed to the source playhead
}

void Session::closeSource() {
    if (source_index_ < 0) return;
    source_index_ = -1;
    source_timeline_.reset();
    emit sourceChanged();
    requestFrame();
}

void Session::markIn() {
    if (source_index_ < 0) return;
    LibraryItem& item = library_[static_cast<std::size_t>(source_index_)];
    item.mark_in = source_frame_;
    if (item.mark_out && *item.mark_out <= source_frame_) item.mark_out.reset(); // a range, never empty
    emit libraryChanged();
    emit sourceChanged();
}

void Session::markOut() {
    if (source_index_ < 0) return;
    LibraryItem& item = library_[static_cast<std::size_t>(source_index_)];
    item.mark_out = source_frame_ + 1;
    if (item.mark_in && *item.mark_in >= *item.mark_out) item.mark_in.reset();
    emit libraryChanged();
    emit sourceChanged();
}

void Session::clearMarks() {
    if (source_index_ < 0) return;
    LibraryItem& item = library_[static_cast<std::size_t>(source_index_)];
    item.mark_in.reset();
    item.mark_out.reset();
    emit libraryChanged();
    emit sourceChanged();
}

QVariantMap Session::source() const {
    if (source_index_ < 0 || !editor_) return {{"open", false}};
    const LibraryItem& item = library_[static_cast<std::size_t>(source_index_)];
    const double fps = frameRate();
    return {{"open", true},
            {"index", source_index_},
            {"name", item.name},
            {"position", static_cast<double>(source_frame_) / fps},
            {"duration", static_cast<double>(source_frames_) / fps},
            {"in", item.mark_in ? static_cast<double>(*item.mark_in) / fps : -1.0},
            {"out", item.mark_out ? static_cast<double>(*item.mark_out) / fps : -1.0}};
}

void Session::toEnd() {
    pause();
    setFrame(lastFrame());
}

void Session::togglePlay() {
    setSpeed(playing() ? 0 : 1);
}

void Session::shuttle(int direction) {
    if (direction > 0) {
        setSpeed(speed_ <= 0 ? 1 : std::min(speed_ * 2, 4));
    } else if (direction < 0) {
        setSpeed(speed_ >= 0 ? -1 : std::max(speed_ * 2, -4));
    }
}

void Session::setSpeed(int speed) {
    if (speed == 0) {
        pause();
        return;
    }
    if (source_index_ >= 0) {
        // ponytail: the source viewer steps and scrubs only; playing it needs its own clock and
        // audio. Starting the sequence here would play something the viewer does not show.
        setNotice(QStringLiteral("The source viewer does not play yet: step with ← → or drag its bar"));
        return;
    }
    if (!hasMedia()) return;
    // From an end, play back into the sequence.
    if (speed > 0 && frame() >= lastFrame()) setFrame(0);
    if (speed < 0 && frame() <= 0) setFrame(lastFrame());
    speed_ = speed;
    startPlayback();
    tick_.start(std::max(4, static_cast<int>(500.0 / frameRate())));
    emit positionChanged();
}

void Session::startPlayback() {
    QElapsedTimer restart;
    restart.start();
    audio_.stop();
    scheduler_.stop();
    wanted_.reset();
    play_from_frame_ = frame();
    clock_.restart();
    if (!snapshot_) refreshSnapshot();
    if (speed_ == 1) {
        // The playhead sits on the frame grid; audio starts at the sample at or before it.
        if (auto r = audio_.start(*snapshot_, *paths_, playhead_ / ticksPerSample()); !r) {
            setNotice(QStringLiteral("Playing without audio: %1").arg(message(r.error())));
        } else if (!audio_.on_device() && !warned_silent_) {
            warned_silent_ = true;
            setNotice(QStringLiteral("No audio device: playing silently"));
        }
    }
    // At double and quadruple speed every second or fourth frame is shown. The hardware pilot
    // decodes on the scheduler's thread too, admitted around Qt's swapchain changes (ADR-0005).
    scheduler_.set_device(hardwarePreview() ? preview_->device() : nullptr);
    if (preview_ != nullptr && preview_quality_ == PreviewQuality::Auto) {
        preview_->setScale(1.0); // each playback starts at full resolution
    }
    adapt_shown_ = 0;
    adapt_dropped_base_ = scheduler_.dropped();
    scheduler_.start(snapshot_, paths_, luts_, canvas_width_, canvas_height_, ticksPerFrame(), frame(), speed_, lastFrame());
    last_restart_ms_ = static_cast<double>(restart.nsecsElapsed()) / 1e6;
}

void Session::pause() {
    if (speed_ == 0) return;
    speed_ = 0;
    tick_.stop();
    audio_.stop();
    scheduler_.stop();
    emit positionChanged();
    requestFrame(); // the exact frame at the playhead, whatever playback last showed
}

std::int64_t Session::ticksPerSample() const {
    const tl::Timeline& t = editor_->timeline();
    return std::max<std::int64_t>(1, t.to_ticks(kSequenceAudioRate.sample_to_time(1)).value_or(1));
}

void Session::refreshSnapshot() {
    if (editor_) snapshot_ = std::make_shared<const tl::Timeline>(editor_->timeline());
}

void Session::refreshPaths() {
    auto paths = std::make_shared<MediaPaths>();
    // A missing file's clips play as gaps (a decoder still open on the moved file must not
    // keep showing it as if nothing were wrong).
    for (const LibraryItem& i : library_) {
        if (!i.missing) paths->emplace(i.media.id.value(), i.path.toStdString());
    }
    paths_ = std::move(paths);
}

void Session::setPreview(PreviewItem* preview) {
    preview_ = preview;
    if (preview_ == nullptr) return;
    // CLAUDE.md §19: degrade visibly. The project, history and saving live on the CPU and keep
    // working; only the viewer and hardware decode stop.
    connect(preview_, &PreviewItem::deviceLost, this, [this] {
        device_lost_ = true;
        setNotice(QString()); // replaces an older notice; the explanation then stays (notice())
    }, Qt::QueuedConnection);
    const QString quality = qEnvironmentVariable("OMA_PREVIEW_SCALE");
    bool ok = false;
    const double fixed = quality.toDouble(&ok);
    preview_quality_from_env_ = ok && fixed > 0.0 && fixed <= 1.0;
    if (ok && fixed > 0.0 && fixed < 1.0) {
        preview_quality_ = PreviewQuality::Fixed;
        preview_->setScale(fixed);
    } else if (ok && fixed == 1.0) {
        preview_quality_ = PreviewQuality::Full;
    } else {
        preview_quality_ = PreviewQuality::Auto; // the default since the 2026-10-06 comparison
    }
}

void Session::setPreviewQuality(const QString& quality) {
    if (preview_ == nullptr || preview_quality_from_env_) return;
    if (quality == QLatin1String("half") || quality == QLatin1String("quarter")) {
        preview_quality_ = PreviewQuality::Fixed;
        preview_->setScale(quality == QLatin1String("half") ? 0.5 : 0.25);
    } else {
        preview_quality_ = quality == QLatin1String("full") ? PreviewQuality::Full : PreviewQuality::Auto;
        preview_->setScale(1.0); // automatic starts at full and lowers only after drops
        adapt_shown_ = 0;
        adapt_dropped_base_ = scheduler_.dropped();
    }
    requestFrame();
}

void Session::setRecordingsFolderOverride(const QString& folder) {
    if (folder == recordings_override_) return;
    recordings_override_ = folder;
    refreshRecordings();
}

// M4 adaptive preview (default; OMA_PREVIEW_SCALE=1 keeps full resolution, 0.5 fixes half): when
// more than 2% of the frames due over two seconds were dropped, composite at half resolution for
// the rest of this playback. The picture's framing, the timeline and export never change.
void Session::adaptPreview() {
    if (preview_ == nullptr || preview_quality_ != PreviewQuality::Auto || preview_->scale() < 1.0) {
        return;
    }
    constexpr std::int64_t kWindowFrames = 120;
    if (++adapt_shown_ < kWindowFrames) {
        return;
    }
    const std::int64_t dropped = scheduler_.dropped() - adapt_dropped_base_;
    if (dropped * 50 > adapt_shown_) {
        preview_->setScale(0.5);
        oma::log_info(oma::Category::Playback, "preview at half resolution: {} of {} frames dropped", dropped,
                      adapt_shown_ + dropped);
    }
    adapt_shown_ = 0;
    adapt_dropped_base_ = scheduler_.dropped();
}

void Session::onTick() {
    QElapsedTimer took;
    took.start();
    if (tick_clock_.isValid() && tick_gap_ms_.size() < 65536) {
        tick_gap_ms_.push_back(static_cast<double>(tick_clock_.nsecsElapsed()) / 1e6);
    }
    tick_clock_.start();
    struct Record {
        QElapsedTimer& t;
        std::vector<double>& out;
        ~Record() {
            if (out.size() < 65536) out.push_back(static_cast<double>(t.nsecsElapsed()) / 1e6);
        }
    } record{took, tick_ms_};
    if (auto error = audio_.take_error()) setNotice(QString::fromStdString(*error));
    if (auto error = scheduler_.take_error()) {
        pause();
        fail(QString::fromStdString(*error));
        return;
    }
    if (speed_ == 1 && audio_.running() && !audio_.healthy()) {
        // The device went away (unplugged, sink removed): restart from what was shown, on a new
        // output; the clock re-anchors there.
        setNotice(QStringLiteral("Audio device changed"));
        startPlayback();
        return;
    }
    std::int64_t target = 0;
    if (speed_ == 1 && audio_.running()) {
        // Video follows the audio actually heard: drop frames when behind, hold when ahead.
        // The selected frame still has to cross the Qt render loop before it reaches the
        // display. Prepare the next frame so that the visible image follows the device clock.
        target = audio_.audible_sample() * ticksPerSample() / ticksPerFrame() + 1;
    } else {
        const double elapsed = static_cast<double>(clock_.nsecsElapsed()) * 1e-9 * frameRate() * speed_;
        target = play_from_frame_ + static_cast<std::int64_t>(std::floor(elapsed));
    }
    const bool at_end = speed_ > 0 ? target >= lastFrame() : target <= 0;
    target = std::clamp<std::int64_t>(target, 0, lastFrame());
    if (auto shown = scheduler_.take(target)) {
        playhead_ = shown->frame * ticksPerFrame();
        if (preview_ != nullptr) preview_->setFrame(std::move(shown->view));
        adaptPreview();
        emit positionChanged();
    }
    if (at_end && frame() == target) pause();
}

// ------------------------------------------------------------------- viewer frames (paused)

void Session::requestFrame() {
    if (playing()) return; // the scheduler feeds the viewer while playing
    if (hardwarePreview()) {
        if (!snapshot_) refreshSnapshot();
        requestHardwareFrame(source_index_ >= 0 ? source_frame_ : frame());
        return;
    }
    wanted_ = playhead_;
    if (!busy_) submitFrame();
}

bool Session::hardwarePreview() const {
    return preview_ != nullptr && preview_->hardwareDecodeAvailable();
}

void Session::requestHardwareFrame(std::int64_t frame) {
    const auto& shown = source_timeline_ ? source_timeline_ : snapshot_;
    if (preview_ != nullptr && shown) {
        preview_->setRequest(shown, paths_, luts_, canvas_width_, canvas_height_, frame,
                             shown->to_ticks(shown->frame_rate().frame_to_time(1)).value_or(1));
    }
}

void Session::submitFrame() {
    if (!wanted_ || !editor_) return;
    std::int64_t frame = *std::exchange(wanted_, std::nullopt) / ticksPerFrame();
    if (!snapshot_) refreshSnapshot();
    auto shown = snapshot_;
    std::int64_t tpf = ticksPerFrame();
    if (source_timeline_) { // the source viewer: its own timeline and playhead
        shown = source_timeline_;
        frame = source_frame_;
        tpf = shown->to_ticks(shown->frame_rate().frame_to_time(1)).value_or(1);
    }
    busy_ = true;
    const unsigned generation = generation_;
    frame_job_ = workers_.submit("viewer-frame", [this, generation, frame, timeline = shown, paths = paths_, luts = luts_,
                                                  width = canvas_width_, height = canvas_height_,
                                                  tpf](oma::JobContext&) {
        auto view = build_viewer_frame(*timeline, *paths, *luts, width, height, frame, tpf, frames_);
        QMetaObject::invokeMethod(this, [this, generation, view = std::move(view)]() mutable {
            busy_ = false;
            if (generation != generation_) return;
            if (!view) {
                fail(message(view.error()));
            } else if (preview_ != nullptr && !playing()) {
                preview_->setFrame(std::move(*view));
            }
            if (wanted_) submitFrame();
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    });
}

// ------------------------------------------------------------------- status

QString Session::notice() const {
    if (notice_.isEmpty() && device_lost_) {
        return QStringLiteral("The graphics device stopped working, so the viewer is blank. Your project is safe: "
                              "save it, then restart OmaMovie.");
    }
    return notice_;
}

void Session::setNotice(const QString& text) {
    notice_ = text;
    notice_timer_.start(kNoticeMs);
    emit statusChanged();
}

void Session::fail(const QString& text) {
    failed_ = true;
    status_ = text;
    emit statusChanged();
}
