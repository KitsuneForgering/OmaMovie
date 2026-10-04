#pragma once

#include "audio_player.hpp"
#include "frame_source.hpp"
#include "video_scheduler.hpp"
#include "viewer_frame.hpp"
#include "waveform_store.hpp"

#include "oma/base/jobs.hpp"
#include "oma/base/time.hpp"
#include "oma/media/format.hpp"
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
    // Waveforms of the library's media, for the timeline's WaveformItems.
    Q_PROPERTY(QObject* waveforms READ waveforms CONSTANT)
    Q_PROPERTY(double selectedClip READ selectedClip NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY selectionChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY sequenceChanged)
    Q_PROPERTY(double duration READ duration NOTIFY sequenceChanged)
    Q_PROPERTY(double frameRate READ frameRate NOTIFY sequenceChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY sequenceChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY sequenceChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY sequenceChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY sequenceChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
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
    [[nodiscard]] double selectedClip() const { return static_cast<double>(selected_clip_.value()); }
    [[nodiscard]] QVariantMap info() const;
    [[nodiscard]] bool hasMedia() const;
    [[nodiscard]] double duration() const;
    [[nodiscard]] double frameRate() const;
    [[nodiscard]] bool canUndo() const { return editor_ && editor_->can_undo(); }
    [[nodiscard]] bool canRedo() const { return editor_ && editor_->can_redo(); }
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;
    [[nodiscard]] double position() const;
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
    Q_INVOKABLE void open(const QString& path) { importFile(path, true); }
    Q_INVOKABLE void importUrl(const QUrl& url);

    Q_INVOKABLE void selectMedia(int index);
    Q_INVOKABLE void selectClip(double id);

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
    // Detaches the selected storyline clip's sound onto an audio lane (a new one if no lane has
    // room), so picture and sound can be trimmed apart for J- and L-cuts.
    Q_INVOKABLE void detachAudio();
    // Volume drawer (ui-design §6): one command per committed change. Fades are snapped to
    // whole frames and clamped so both fit in the clip.
    Q_INVOKABLE void setClipAudio(double gain, double fadeIn, double fadeOut, bool muted);
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
    };

    void importFile(const QString& path, bool append);
    void addToLibrary(LibraryItem item, bool append);
    [[nodiscard]] const LibraryItem* item(oma::timeline::MediaId id) const;
    bool ensureSequence(const LibraryItem& first);
    [[nodiscard]] std::optional<oma::timeline::edit::ClipSource> sourceFor(const LibraryItem& item) const;
    bool run(std::unique_ptr<oma::timeline::Command> command);
    void afterEdit();
    void placeSelected(int how);
    bool placeAudio(int how, oma::timeline::ClipId id, const oma::timeline::edit::ClipSource& clip);
    [[nodiscard]] std::vector<oma::timeline::TrackId> audioLanes() const;
    [[nodiscard]] QVariantMap clipMap(const oma::timeline::Clip& c) const;
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

    PreviewItem* preview_ = nullptr;
    bool editing_ = false;
    QString status_;
    bool failed_ = false;
    QString notice_;
    QTimer notice_timer_;

    std::vector<LibraryItem> library_;
    int selected_media_ = -1;
    std::uint64_t next_media_ = 1;

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

    // Viewer frame requests: one in flight, and only the latest wish kept.
    bool busy_ = false;
    std::optional<std::int64_t> wanted_;
    unsigned generation_ = 0;

    // Imports in flight (cancelled by New project) and the viewer frame being decoded.
    std::vector<oma::JobHandle> imports_;
    oma::JobHandle frame_job_;

    QTemporaryDir thumbnails_;
    // Accessed only by the import job worker; its lifetime extends past worker shutdown.
    std::unique_ptr<oma::media::MediaImporter> importer_;
    FrameSource frames_;      // job worker only
    AudioPlayer audio_;
    WaveformStore waveforms_;
    bool warned_silent_ = false;
    oma::JobPool workers_{1}; // destroyed first: no job outlives what it touches
};
