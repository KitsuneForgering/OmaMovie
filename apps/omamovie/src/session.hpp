#pragma once

#include "audio_player.hpp"
#include "frame_source.hpp"
#include "video_scheduler.hpp"
#include "viewer_frame.hpp"
#include "waveform_store.hpp"

#include "oma/base/jobs.hpp"
#include "oma/base/time.hpp"
#include "oma/media/format.hpp"
#include "oma/project/document.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

class PreviewItem;

// The bridge between QML and the editing engine (CLAUDE.md §10–§11): the media library, the
// M5 timeline Editor (every edit a command, with undo/redo), the playhead and the viewer.
// QML reads properties and calls intents; it holds no timeline state of its own.
//
// Threading: lives on the UI thread. Probing, thumbnails and viewer frames run on a single
// job worker, which alone touches `frames_`; results come back through queued invocations and
// are dropped when a newer project replaced the one that asked (`generation_`).
//
// Time: the playhead is an integer tick of the sequence timebase, always on the frame grid.
// Seconds cross into QML for display only.
class Session final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing NOTIFY viewChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY statusChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY statusChanged)
    Q_PROPERTY(QVariantList media READ media NOTIFY libraryChanged)
    Q_PROPERTY(int selectedMedia READ selectedMedia NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY sequenceChanged)
    // Audio lanes below the storyline, top first: {id, name, clips}.
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY sequenceChanged)
    // Previews of the selected clip under each filter (Effects drawer), FilterKind order.
    Q_PROPERTY(QVariantList filterPreviews READ filterPreviews NOTIFY filterPreviewsChanged)
    // LUTs loaded in this project (ADR-0012): {id, name}.
    Q_PROPERTY(QVariantList luts READ luts NOTIFY lutsChanged)
    // Waveforms of the library's media, for the timeline's WaveformItems.
    Q_PROPERTY(QObject* waveforms READ waveforms CONSTANT)
    Q_PROPERTY(double selectedClip READ selectedClip NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY selectionChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY sequenceChanged)
    // The project file (ADR-0007): its name ("Untitled project" before the first save), its
    // path, and whether the session changed since it was saved or opened.
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY projectChanged)
    Q_PROPERTY(double duration READ duration NOTIFY sequenceChanged)
    Q_PROPERTY(double frameRate READ frameRate NOTIFY sequenceChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth NOTIFY sequenceChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight NOTIFY sequenceChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY sequenceChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY sequenceChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY sequenceChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY sequenceChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    // The selected clip's transform at the playhead (keyframes evaluated): posX, posY, scale,
    // rotation; `keys` how many transform keys it has, `keyHere` whether one is at the playhead.
    Q_PROPERTY(QVariantMap motion READ motion NOTIFY motionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY positionChanged)
    Q_PROPERTY(int speed READ speed NOTIFY positionChanged)

public:
    explicit Session(QObject* parent = nullptr);
    explicit Session(std::unique_ptr<oma::media::MediaImporter> importer, QObject* parent = nullptr);
    ~Session() override;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    void setPreview(PreviewItem* preview) { preview_ = preview; }
    // Automated runs: never play through the speakers.
    void setSilent(bool silent) { audio_.set_silent(silent); }
    void setOutputMuted(bool muted) { audio_.set_muted(muted); }
    [[nodiscard]] float takeAudioPeak() { return audio_.take_peak(); }
    [[nodiscard]] bool audioRunning() const { return audio_.running(); }
    [[nodiscard]] bool audioOnDevice() const { return audio_.on_device(); }
    [[nodiscard]] std::int64_t audibleSample() const { return audio_.audible_sample(); }
    [[nodiscard]] std::int64_t audioUnderruns() const { return audio_.underruns(); }
    [[nodiscard]] std::int64_t droppedVideoFrames() const { return scheduler_.dropped(); }

    [[nodiscard]] bool editing() const { return editing_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] QString notice() const { return notice_; }
    [[nodiscard]] QVariantList media() const;
    [[nodiscard]] int selectedMedia() const { return selected_media_; }
    [[nodiscard]] QVariantList clips() const;
    [[nodiscard]] QVariantList audioTracks() const;
    [[nodiscard]] QObject* waveforms() { return &waveforms_; }
    [[nodiscard]] QVariantList filterPreviews() const { return filter_previews_; }
    [[nodiscard]] QVariantList luts() const;
    [[nodiscard]] double selectedClip() const { return static_cast<double>(selected_clip_.value()); }
    [[nodiscard]] QVariantMap info() const;
    [[nodiscard]] bool hasMedia() const;
    [[nodiscard]] double duration() const;
    [[nodiscard]] double frameRate() const;
    [[nodiscard]] int canvasWidth() const { return static_cast<int>(canvas_width_); }
    [[nodiscard]] int canvasHeight() const { return static_cast<int>(canvas_height_); }
    [[nodiscard]] bool canUndo() const { return editor_ && editor_->can_undo(); }
    [[nodiscard]] bool canRedo() const { return editor_ && editor_->can_redo(); }
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;
    [[nodiscard]] double position() const;
    [[nodiscard]] QVariantMap motion() const;
    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QString projectPath() const { return project_path_; }
    [[nodiscard]] bool dirty() const { return savedMarker() != saved_; }
    [[nodiscard]] bool playing() const { return speed_ != 0; }
    // Playback speed: 0 paused, 1 normal, 2 and 4 faster, negative in reverse (J/K/L).
    [[nodiscard]] int speed() const { return speed_; }
    [[nodiscard]] std::int64_t frame() const;

    // Screens (ui-design §3).
    Q_INVOKABLE void newProject();
    Q_INVOKABLE void showProjects();
    Q_INVOKABLE void continueProject();

    // Import. A file opened with the app goes straight to the end of the storyline (§3.1); one
    // imported from the dialog goes to the library, and to the storyline only when it is empty.
    // A project file (*.omamovie) opens as the project instead.
    Q_INVOKABLE void open(const QString& path);
    // Saves in the background (atomic replacement); an empty url saves to the current file.
    Q_INVOKABLE void saveProject(const QUrl& url);
    // Replaces the session with a saved project: its library is imported again (thumbnails,
    // details) under the saved IDs, the timeline is restored as saved.
    Q_INVOKABLE void openProject(const QUrl& url);
    Q_INVOKABLE void importUrl(const QUrl& url);

    Q_INVOKABLE void selectMedia(int index);
    Q_INVOKABLE void selectClip(double id);
    // Keyboard clip navigation (ui-design §8.1): selects the storyline clip before (-1) or after
    // (+1) the selected one, or the one under the playhead, and moves the playhead to its start.
    Q_INVOKABLE void selectAdjacentClip(int direction);

    // Edits (ui-design §8.1). Each is one command in the history.
    Q_INVOKABLE void appendSelected();
    Q_INVOKABLE void insertSelected();
    Q_INVOKABLE void overwriteSelected();
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void deleteSelected(bool ripple);
    // Drags an edge by whole frames; the storyline stays magnetic (ripple, ui-design §7.3).
    Q_INVOKABLE void trimClip(double id, bool head, int frames);
    // Moves a clip on the lanes below the storyline by whole lanes (a new lane past the last)
    // and frames; the storyline reorders by editing instead.
    Q_INVOKABLE void moveClip(double id, int lanes, int frames);
    // Drag and drop (ui-design §7.3). The storyline is magnetic: a drop lands on the cut nearest
    // `seconds`. storylineCut answers which one, for the drop marker; `moving` is the clip being
    // dragged (0 for none), whose own edges are where it already is.
    Q_INVOKABLE double storylineCut(double seconds, double moving) const;
    // Snapping (ui-design §7.2): where a span of `length` seconds dragged to `seconds` lands
    // when either end comes within `tolerance` of a clip edge on any track (other than clip
    // `exclude`), the playhead or zero. Returns {start, line}: the adjusted start and the edge
    // it snapped to, or line -1 when nothing is in reach and start is unchanged.
    Q_INVOKABLE QVariantMap snapSpan(double seconds, double length, double exclude, double tolerance) const;
    // Moves a storyline clip to the cut nearest `seconds`, closing its old place.
    Q_INVOKABLE void reorderClip(double id, double seconds);
    // Places library item `index` dropped at `seconds`: pictures are inserted on the storyline at
    // the nearest cut; sound goes on audio lane `lane` (top first) if it has room there, else on
    // the first lane with room, else on a new lane.
    Q_INVOKABLE void dropMedia(int index, double seconds, int lane);
    // Detaches the selected storyline clip's sound onto an audio lane (a new one if no lane has
    // room), so picture and sound can be trimmed apart for J- and L-cuts.
    Q_INVOKABLE void detachAudio();
    // Volume drawer (ui-design §6): one command per committed change. Fades are snapped to
    // whole frames and clamped so both fit in the clip.
    Q_INVOKABLE void setClipAudio(double gain, double fadeIn, double fadeOut, bool muted);
    // Volume "More" (ui-design §6, level 2), each one command. Equalizer gains in dB.
    Q_INVOKABLE void setClipEq(double low, double mid, double high);
    // Noise reduction amount in [0, 1]. The noise level comes from the clip's quiet parts, so it
    // needs the clip's waveform (analysis runs after import).
    Q_INVOKABLE void setClipNoise(double amount);
    // Sets the volume so the clip's loudest peak reaches -1 dBFS.
    Q_INVOKABLE void normalizeClip();

    // Video adjustments of the selected clip (ui-design §6), each one command.
    // Color: exposure in stops, the others in [-1, 1].
    Q_INVOKABLE void setClipColor(double exposure, double contrast, double saturation, double temperature);
    // Effects: a FilterKind and its amount in [0, 1].
    Q_INVOKABLE void setClipFilter(int kind, double amount);
    // Blur (below 0) or sharpen (above 0), in [-1, 1].
    Q_INVOKABLE void setClipSharpness(double sharpness);
    // Crop and framing: a Fit mode and the fractions cropped from each edge.
    Q_INVOKABLE void setClipFraming(int fit, double left, double top, double right, double bottom);
    // Position in output pixels from the center, uniform scale, clockwise rotation in degrees.
    // With transform keys (M8 keyframes) it sets the key at the playhead, adding one if needed.
    Q_INVOKABLE void setClipTransform(double x, double y, double scale, double rotation);
    // Adds a transform key at the playhead with the transform shown there, or removes the one
    // there (the last one removed leaves its transform as the clip's).
    Q_INVOKABLE void toggleTransformKey();
    // Ken Burns: a slow push in over the whole clip, from the transform shown at its start to
    // 120% of it at its end, eased.
    Q_INVOKABLE void kenBurns();
    // Fit, no crop, no transform or keys: one command.
    Q_INVOKABLE void resetClipFraming();
    // The transition into a storyline clip from the clip before it: a TransitionKind, or -1 to
    // remove it, lasting `seconds` (snapped to whole frames).
    Q_INVOKABLE void setTransition(double clip, int kind, double seconds);
    // Adds a one-second cross dissolve at the storyline cut nearest the playhead.
    Q_INVOKABLE void addDissolveAtPlayhead();
    // Color grading of the selected clip (ADR-0012, Color drawer "More"), each one command.
    // Lift, gamma and gain wheels: {lift, gamma, gain}, each {x, y} in the unit disc (the tint)
    // and a level in [-1, 1]. Stored as the ASC CDL they map onto exactly.
    Q_INVOKABLE void setClipWheels(const QVariantMap& wheels);
    // Curves: channel 0 master, 1-3 red, green, blue; coordinates in [0, 1]. The first point
    // added to an empty curve also adds its two ends.
    Q_INVOKABLE void addCurvePoint(int channel, double x, double y);
    // Moves a point, kept between its neighbours.
    Q_INVOKABLE void moveCurvePoint(int channel, int index, double x, double y);
    // Removes an inner point; removing the last inner one leaves the curve straight.
    Q_INVOKABLE void removeCurvePoint(int channel, int index);
    // `count` values of a curve across [0, 1], for drawing it.
    Q_INVOKABLE QVariantList curveSamples(int channel, int count) const;
    // Loads a .cube file in the background, adds it to the project's LUTs and applies it to
    // the selected clip.
    Q_INVOKABLE void importLut(const QUrl& url);
    // The selected clip's LUT (0 for none) and how much of it, in [0, 1].
    Q_INVOKABLE void setClipLut(double id, double amount);
    // Removes wheels, curves and LUT from the selected clip.
    Q_INVOKABLE void resetClipGrade();
    // Renders the selected clip's first frame under every filter, in the background.
    Q_INVOKABLE void requestFilterPreviews();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Transport.
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void pause();
    // J/K/L (ui-design §8.1): +1 is L (play forward, faster on repeat), -1 is J (reverse,
    // faster on repeat); K is pause(). Audio plays only at normal speed; at other speeds the
    // clock is monotonic.
    Q_INVOKABLE void shuttle(int direction);
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void stepFrames(int frames);
    Q_INVOKABLE void toEnd();

signals:
    void viewChanged();
    void statusChanged();
    void libraryChanged();
    void selectionChanged();
    void sequenceChanged();
    void positionChanged();
    void motionChanged();
    void projectChanged();
    void filterPreviewsChanged();
    void lutsChanged();

private:
    struct LibraryItem {
        oma::timeline::MediaInfo media;
        QString path;
        QString name;
        bool audio_only = false; // music, voiceover, sound effects: placed on the audio lanes
        QString thumbnail;
        double seconds = 0; // display only
        std::optional<oma::FrameRate> rate;
        std::uint32_t width = 0;  // display size, rotation applied
        std::uint32_t height = 0;
        QVariantMap details; // codec, resolution, frame rate for the Info drawer
        oma::project::Fingerprint fingerprint; // for relinking (ADR-0007)
    };

    // `known`: a library item of an opened project, imported again under its saved ID and
    // media range (the restored timeline references them).
    void importFile(const QString& path, bool append, std::optional<oma::project::MediaRef> known = std::nullopt);
    void applyProject(oma::project::Document doc, const QString& path);
    // A LUT of an opened project: parsed in the background into the tables under its saved ID.
    void loadLut(const QString& path, oma::timeline::LutId id);
    // What saving records: timeline, revision, library and LUT counts. Equal to saved_ when
    // nothing changed since the last save or open.
    using Marker = std::tuple<bool, std::uint64_t, std::size_t, std::size_t>;
    [[nodiscard]] Marker savedMarker() const;
    void addToLibrary(LibraryItem item, bool append);
    [[nodiscard]] const LibraryItem* item(oma::timeline::MediaId id) const;
    bool ensureSequence(const LibraryItem& first);
    [[nodiscard]] std::optional<oma::timeline::edit::ClipSource> sourceFor(const LibraryItem& item) const;
    bool run(std::unique_ptr<oma::timeline::Command> command);
    void afterEdit();
    void placeSelected(int how);
    bool placeAudio(int how, oma::timeline::ClipId id, const oma::timeline::edit::ClipSource& clip,
                    std::int64_t start, std::optional<std::size_t> preferred = std::nullopt);
    [[nodiscard]] std::int64_t ticksAt(double seconds) const;
    // The selected clip's exact source time at the playhead, clamped into the clip.
    [[nodiscard]] std::optional<oma::RationalTime> keyTime(const oma::timeline::Clip& c) const;
    [[nodiscard]] std::int64_t nearestCut(std::int64_t ticks, oma::timeline::ClipId moving) const;
    [[nodiscard]] std::vector<oma::timeline::TrackId> audioLanes() const;
    [[nodiscard]] QVariantMap clipMap(const oma::timeline::Clip& c, const oma::timeline::Track& track,
                                       std::size_t index) const;
    // The selected clip's waveform and the media seconds it shows, if analysed.
    struct ClipSound {
        std::shared_ptr<const oma::playback::Waveform> waveform;
        double from = 0; // in the waveform's own clock
        double to = 0;
    };
    [[nodiscard]] std::optional<ClipSound> selectedSound() const;
    void setSelectedAudio(const oma::timeline::AudioProperties& audio);
    void setSelectedVideo(const oma::timeline::VideoProperties& video);
    void setNotice(const QString& text);
    void fail(const QString& message);

    [[nodiscard]] std::int64_t ticksPerFrame() const;
    [[nodiscard]] std::int64_t lastFrame() const;
    void setFrame(std::int64_t frame);
    void startPlayback();
    void setSpeed(int speed);
    void refreshSnapshot();
    void refreshPaths();
    [[nodiscard]] std::int64_t ticksPerSample() const;
    void onTick();
    void requestFrame();
    void submitFrame();
    [[nodiscard]] bool hardwarePreview() const;
    void requestHardwareFrame(std::int64_t frame);

    PreviewItem* preview_ = nullptr;
    bool editing_ = false;
    QString status_;
    bool failed_ = false;
    QString notice_;
    QTimer notice_timer_;

    std::vector<LibraryItem> library_;
    int selected_media_ = -1;
    std::uint64_t next_media_ = 1;
    std::uint64_t next_lut_ = 1;
    std::unordered_map<std::uint64_t, QString> lut_paths_; // LUT ID -> its .cube file
    QString project_path_;
    Marker saved_{false, 0, 0, 0};
    void setSelectedCurve(int channel, std::vector<oma::timeline::CurvePoint> points);
    // The lift/gamma/gain wheels a CDL corresponds to (the inverse of setClipWheels).
    [[nodiscard]] static QVariantMap wheels_of(const oma::timeline::Cdl& cdl);

    std::optional<oma::timeline::Editor> editor_;
    oma::timeline::TrackId primary_; // the storyline (ui-design §7.1)
    // The frame size of the sequence, from its first clip (ui-design §3: format from the first
    // clip; the aspect-ratio choice of ADR-0010 comes later).
    std::uint32_t canvas_width_ = 1920;
    std::uint32_t canvas_height_ = 1080;
    oma::timeline::ClipId selected_clip_;

    std::int64_t playhead_ = 0; // sequence ticks, on the frame grid
    int speed_ = 0;
    // The master clock is the audio actually played (CLAUDE.md §12): video follows it and
    // drops frames it cannot decode in time. Only if audio cannot start at all does a monotonic
    // clock take over.
    QElapsedTimer clock_;
    std::int64_t play_from_frame_ = 0;
    QTimer tick_;
    VideoScheduler scheduler_;
    // Immutable views shared with the pipelines: the timeline as of the last edit and the
    // library's files. Replaced, never mutated.
    std::shared_ptr<const oma::timeline::Timeline> snapshot_;
    std::shared_ptr<const MediaPaths> paths_ = std::make_shared<const MediaPaths>();
    std::shared_ptr<const LutTables> luts_ = std::make_shared<const LutTables>(); // replaced, never mutated

    // Viewer frame requests: one in flight, and only the latest wish kept.
    bool busy_ = false;
    std::optional<std::int64_t> wanted_;
    unsigned generation_ = 0;

    // Imports in flight (cancelled by New project) and the viewer frame being decoded.
    std::vector<oma::JobHandle> imports_;
    oma::JobHandle frame_job_;
    oma::JobHandle save_job_; // never cancelled: a save the user asked for completes

    QTemporaryDir thumbnails_;
    // Accessed only by the import job worker; its lifetime extends past worker shutdown.
    std::unique_ptr<oma::media::MediaImporter> importer_;
    FrameSource frames_;      // job worker only
    AudioPlayer audio_;
    WaveformStore waveforms_;
    bool warned_silent_ = false;
    QVariantList filter_previews_;
    unsigned previews_ = 0; // filter preview requests, so a stale result is dropped
    oma::JobHandle previews_job_;
    oma::JobPool workers_{1}; // destroyed first: no job outlives what it touches
};
