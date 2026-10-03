#include "session.hpp"

#include "preview_item.hpp"

#include "oma/compositor/render_graph.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/evaluate.hpp"

#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

namespace tl = oma::timeline;

namespace {

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
      importer_(importer ? std::move(importer) : std::make_unique<oma::media::FfmpegFormatBackend>()),
      audio_(kSequenceAudioRate) {
    notice_timer_.setSingleShot(true);
    connect(&notice_timer_, &QTimer::timeout, this, [this] {
        notice_.clear();
        emit statusChanged();
    });
    tick_.setTimerType(Qt::PreciseTimer);
    connect(&tick_, &QTimer::timeout, this, &Session::onTick);
}

Session::~Session() {
    workers_.shutdown();
}

// ------------------------------------------------------------------- screens

void Session::newProject() {
    ++generation_;
    pause();
    for (oma::JobHandle& h : imports_) h.cancel();
    imports_.clear();
    library_.clear();
    editor_.reset();
    snapshot_.reset();
    refreshPaths();
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

void Session::importFile(const QString& path, bool append) {
    if (path.isEmpty()) return;
    if (!editing_) {
        editing_ = true;
        emit viewChanged();
    }
    setNotice(QStringLiteral("Importing %1…").arg(QFileInfo(path).fileName()));
    const unsigned generation = generation_;
    const tl::MediaId id(next_media_++);
    const QString thumbnail_path =
        thumbnails_.filePath(QString::number(id.value()) + QStringLiteral(".png"));
    std::erase_if(imports_, [](const oma::JobHandle& h) {
        const auto s = h.state();
        return s != oma::JobState::Pending && s != oma::JobState::Running;
    });
    imports_.push_back(workers_.submit("import", [this, generation, id, path, thumbnail_path, append](oma::JobContext&) {
        const std::string file = path.toStdString();
        auto probed = importer_->inspect_input(std::filesystem::path(file));
        if (!probed || (!probed->best_video && !probed->best_audio)) {
            const QString why = probed ? QStringLiteral("no video or audio stream") : message(probed.error());
            QMetaObject::invokeMethod(this, [this, generation, path, why] {
                if (generation == generation_)
                    fail(QStringLiteral("Cannot import %1: %2").arg(QFileInfo(path).fileName(), why));
            }, Qt::QueuedConnection);
            return oma::Result<void>{};
        }
        // Music, voiceover and sound effects are audio-only media; they go on the audio lanes.
        const bool audio_only = !probed->best_video;
        const auto& stream =
            probed->streams[static_cast<std::size_t>(audio_only ? *probed->best_audio : *probed->best_video)];
        const oma::Rational tb = stream.timebase;
        LibraryItem item;
        item.path = path;
        item.name = QFileInfo(path).fileName();
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
        // Audio-only media has no picture: the library and the lanes show the sound itself.
        if (!audio_only) {
            if (auto first = frames_.image_at(file, item.media.start)) {
                if (first->scaled(256, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(thumbnail_path))
                    item.thumbnail = QUrl::fromLocalFile(thumbnail_path).toString();
            }
        }
        QMetaObject::invokeMethod(this, [this, generation, item = std::move(item), append]() mutable {
            if (generation == generation_) addToLibrary(std::move(item), append);
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

void Session::addToLibrary(LibraryItem item, bool append) {
    if (editor_) {
        if (auto r = editor_->add_media(item.media); !r) {
            fail(message(r.error()));
            return;
        }
    }
    library_.push_back(std::move(item));
    refreshPaths();
    selected_media_ = static_cast<int>(library_.size()) - 1;
    notice_.clear();
    emit statusChanged();
    emit libraryChanged();
    emit selectionChanged();
    if (append) appendSelected();
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
    return tl::edit::ClipSource{.media = item.media.id,
                                .source_in = item.media.start,
                                .duration = t.at(*ticks),
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
    const bool done = source.audio_only ? placeAudio(how, id, *clip)
                      : how == 0        ? run(tl::edit::append(primary_, id, *clip))
                      : how == 1        ? run(tl::edit::insert(primary_, id, at, *clip))
                                        : run(tl::edit::overwrite(primary_, id, at, *clip));
    if (done) {
        selected_clip_ = id;
        emit selectionChanged();
    }
}

// Sound goes below the storyline (ui-design §7.1): append after the first lane's last clip,
// insert as a new connected clip at the playhead on the first lane with room (a new lane if
// none has), overwrite on the first lane. Lanes appear as needed, in the same history entry.
bool Session::placeAudio(int how, tl::ClipId id, const tl::edit::ClipSource& clip) {
    const tl::Timeline& t = editor_->timeline();
    const auto lanes = audioLanes();
    const std::int64_t start = playhead_;
    const std::int64_t end = start + clip.duration.value();
    std::optional<tl::TrackId> lane;
    if (how == 1) {
        for (const tl::TrackId l : lanes) {
            const auto& clips = t.find_track(l)->clips;
            if (std::ranges::none_of(clips, [&](const tl::Clip& c) {
                    return c.start_ticks() < end && start < c.end_ticks();
                })) {
                lane = l;
                break;
            }
        }
    } else if (!lanes.empty()) {
        lane = lanes.front();
    }
    std::vector<std::unique_ptr<tl::Command>> steps;
    if (!lane) {
        lane = editor_->new_track_id();
        steps.push_back(tl::edit::add_track(*lane, tl::TrackKind::Audio,
                                            "Audio " + std::to_string(lanes.size() + 1)));
    }
    steps.push_back(how == 0 ? tl::edit::append(*lane, id, clip) : tl::edit::overwrite(*lane, id, t.at(start), clip));
    return run(tl::edit::transaction(how == 0 ? "Append Audio" : how == 1 ? "Connect Audio" : "Overwrite Audio",
                                     std::move(steps)));
}

std::vector<tl::TrackId> Session::audioLanes() const {
    std::vector<tl::TrackId> lanes;
    if (!editor_) return lanes;
    for (const tl::Track& track : editor_->timeline().tracks()) {
        if (track.kind == tl::TrackKind::Audio) lanes.push_back(track.id);
    }
    return lanes;
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

void Session::deleteSelected(bool ripple) {
    if (!editor_) return;
    tl::ClipId id = selected_clip_;
    if (!id.valid()) {
        const tl::Clip* c = editor_->timeline().clip_at(primary_, playhead_);
        if (c == nullptr) return;
        id = c->id;
    }
    const tl::Track* track = editor_->timeline().track_of(id);
    if (track == nullptr) return;
    if (track->id == primary_) {
        run(ripple ? tl::edit::ripple_delete(id) : tl::edit::remove_clip(id));
        return;
    }
    // Lanes below and above the storyline are not magnetic: deleting leaves the time free.
    std::vector<std::unique_ptr<tl::Command>> steps;
    steps.push_back(tl::edit::remove_clip(id));
    if (track->clips.size() == 1) steps.push_back(tl::edit::remove_track(track->id));
    run(tl::edit::transaction("Delete", std::move(steps)));
}

void Session::trimClip(double id, bool head, int frames) {
    if (!editor_ || frames == 0) return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    const tl::Clip* c = editor_->timeline().find_clip(clip);
    if (c == nullptr) return;
    const std::int64_t delta = static_cast<std::int64_t>(frames) * ticksPerFrame();
    const tl::Timeline& t = editor_->timeline();
    const bool magnetic = t.track_of(clip) != nullptr && t.track_of(clip)->id == primary_;
    if (run(head ? tl::edit::trim_start(clip, t.at(c->start_ticks() + delta), magnetic)
                 : tl::edit::trim_end(clip, t.at(c->end_ticks() + delta), magnetic))) {
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
    const auto all = audioLanes();
    const auto index = std::ranges::find(all, from->id) - all.begin();
    const auto target = std::max<std::ptrdiff_t>(0, index + lanes);
    const std::int64_t start = std::max<std::int64_t>(0, c->start_ticks() + (static_cast<std::int64_t>(frames) * ticksPerFrame()));
    std::vector<std::unique_ptr<tl::Command>> steps;
    tl::TrackId to;
    if (target < static_cast<std::ptrdiff_t>(all.size())) {
        to = all[static_cast<std::size_t>(target)];
    } else {
        to = editor_->new_track_id();
        steps.push_back(tl::edit::add_track(to, tl::TrackKind::Audio, "Audio " + std::to_string(all.size() + 1)));
    }
    if (to == from->id && start == c->start_ticks()) return;
    steps.push_back(tl::edit::move_clip(clip, to, t.at(start)));
    if (to != from->id && from->clips.size() == 1) steps.push_back(tl::edit::remove_track(from->id));
    if (run(tl::edit::transaction("Move", std::move(steps)))) {
        selected_clip_ = clip;
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
                                   {"thumbnail", i.thumbnail}});
    }
    return list;
}

QVariantMap Session::clipMap(const tl::Clip& c) const {
    const LibraryItem* source = item(c.media);
    return QVariantMap{{"id", static_cast<double>(c.id.value())},
                       {"start", c.start.seconds_approx()},
                       {"duration", c.duration.seconds_approx()},
                       {"name", source != nullptr ? source->name : QString()},
                       {"thumbnail", source != nullptr ? source->thumbnail : QString()},
                       {"fadeIn", c.audio.fade_in.seconds_approx()},
                       {"fadeOut", c.audio.fade_out.seconds_approx()},
                       {"audioAdjusted", c.audio.muted || c.audio.gain != 1.0F || c.audio.fade_in.value() != 0 ||
                                             c.audio.fade_out.value() != 0}};
}

QVariantList Session::clips() const {
    QVariantList list;
    if (!editor_) return list;
    const tl::Track* track = editor_->timeline().find_track(primary_);
    if (track == nullptr) return list;
    for (const tl::Clip& c : track->clips) list.push_back(clipMap(c));
    return list;
}

QVariantList Session::audioTracks() const {
    QVariantList lanes;
    for (const tl::TrackId id : audioLanes()) {
        const tl::Track* track = editor_->timeline().find_track(id);
        QVariantList clips;
        for (const tl::Clip& c : track->clips) clips.push_back(clipMap(c));
        lanes.push_back(QVariantMap{{"id", static_cast<double>(id.value())},
                                    {"name", QString::fromStdString(track->name)},
                                    {"clips", clips}});
    }
    return lanes;
}

QVariantMap Session::info() const {
    const LibraryItem* source = nullptr;
    if (editor_ && selected_clip_.valid()) {
        if (const tl::Clip* c = editor_->timeline().find_clip(selected_clip_)) source = item(c->media);
    }
    if (source == nullptr && selected_media_ >= 0 && selected_media_ < static_cast<int>(library_.size()))
        source = &library_[static_cast<std::size_t>(selected_media_)];
    if (source == nullptr) return {};
    QVariantMap out = source->details;
    out.insert("name", source->name);
    out.insert("duration", source->seconds);
    out.insert("hasAudio", source->media.has_audio);
    if (editor_ && selected_clip_.valid()) {
        if (const tl::Clip* c = editor_->timeline().find_clip(selected_clip_)) {
            out.insert("clip", true);
            out.insert("clipDuration", c->duration.seconds_approx());
            out.insert("gain", static_cast<double>(c->audio.gain));
            out.insert("fadeIn", c->audio.fade_in.seconds_approx());
            out.insert("fadeOut", c->audio.fade_out.seconds_approx());
            out.insert("muted", c->audio.muted);
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
    playhead_ = std::clamp<std::int64_t>(frame, 0, lastFrame()) * ticksPerFrame();
    emit positionChanged();
    requestFrame();
}

void Session::seek(double seconds) {
    if (!editor_ || !std::isfinite(seconds)) return;
    // Display seconds to the nearest frame of the sequence grid.
    setFrame(std::llround(seconds * frameRate()));
    if (playing()) startPlayback(); // restart the clock, audio and decode-ahead from here
}

void Session::stepFrames(int frames) {
    pause();
    setFrame(frame() + frames);
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
    // At double and quadruple speed every second or fourth frame is shown.
    scheduler_.start(snapshot_, paths_, canvas_width_, canvas_height_, ticksPerFrame(), frame(), speed_, lastFrame());
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
    for (const LibraryItem& i : library_) paths->emplace(i.media.id.value(), i.path.toStdString());
    paths_ = std::move(paths);
}

void Session::onTick() {
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
        emit positionChanged();
    }
    if (at_end && frame() == target) pause();
}

// ------------------------------------------------------------------- viewer frames (paused)

void Session::requestFrame() {
    if (playing()) return; // the scheduler feeds the viewer while playing
    wanted_ = playhead_;
    if (!busy_) submitFrame();
}

void Session::submitFrame() {
    if (!wanted_ || !editor_) return;
    const std::int64_t frame = *std::exchange(wanted_, std::nullopt) / ticksPerFrame();
    if (!snapshot_) refreshSnapshot();
    busy_ = true;
    const unsigned generation = generation_;
    frame_job_ = workers_.submit("viewer-frame", [this, generation, frame, timeline = snapshot_, paths = paths_,
                                                  width = canvas_width_, height = canvas_height_,
                                                  tpf = ticksPerFrame()](oma::JobContext&) {
        auto view = build_viewer_frame(*timeline, *paths, width, height, frame, tpf, frames_);
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
