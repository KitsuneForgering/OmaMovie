#pragma once

#include "audio_player.hpp"
#include "clip_list_model.hpp"
#include "recent_projects.hpp"
#include "frame_source.hpp"
#include "video_scheduler.hpp"
#include "viewer_frame.hpp"
#include "waveform_store.hpp"

#include "oma/base/disk_cache.hpp"
#include "exporter.hpp"
#include "oma/base/jobs.hpp"
#include "oma/base/time.hpp"
#include "oma/media/format.hpp"
#include "oma/project/document.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <limits>
#include <atomic>
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
    // Export (M7): progress in [0, 1] while an export runs, -1 otherwise; the last finished file.
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportChanged)
    Q_PROPERTY(QString exportedFile READ exportedFile NOTIFY exportChanged)
    Q_PROPERTY(QVariantList media READ media NOTIFY libraryChanged)
    // Omarchy screen recordings, newest first (UX-09): path, name, when, growing.
    Q_PROPERTY(QVariantList recordings READ recordings NOTIFY recordingsChanged)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(QVariantList recoveredProjects READ recoveredProjects NOTIFY recoveredChanged)
    Q_PROPERTY(QString recordingsFolder READ recordingsFolder NOTIFY recordingsChanged)
    Q_PROPERTY(int selectedMedia READ selectedMedia NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY sequenceChanged)
    // The same storyline clips as a model that keeps delegates across edits (long timelines).
    Q_PROPERTY(QObject* storylineClips READ storylineClips CONSTANT)
    // More storyline clips in view than delegates make sense for: the timeline draws them as one
    // strip (storylineClips is then empty). Set from the view's range (setStorylineView).
    Q_PROPERTY(bool storylineCompact READ storylineCompact NOTIFY storylineViewChanged)
    // The storyline as [start, duration, ...] seconds, and the selected clip's [start, duration]:
    // the minimap draws thousands of clips from numbers rather than one map per clip.
    Q_PROPERTY(QList<double> clipSpans READ clipSpans NOTIFY sequenceChanged)
    Q_PROPERTY(QList<double> selectedSpan READ selectedSpan NOTIFY selectionChanged)
    // Audio lanes below the storyline, top first: {id, name, clips}.
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY sequenceChanged)
    // Video lanes above the storyline, nearest first: titles, cutaways, picture in picture.
    Q_PROPERTY(QVariantList videoTracks READ videoTracks NOTIFY sequenceChanged)
    // Previews of the selected clip under each look, in lookEffects order (Effects drawer).
    Q_PROPERTY(QVariantList filterPreviews READ filterPreviews NOTIFY filterPreviewsChanged)
    // The built-in looks (ADR-0016): {id, name}. QML keeps no effect table of its own.
    Q_PROPERTY(QVariantList lookEffects READ lookEffects CONSTANT)
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
    // The source viewer (M6 pilot, VEGAS-style trimmer): a library item shown on its own with
    // its own playhead and in/out marks; Add/Insert/Overwrite/Connect then use the marked range.
    // {open, index, name, position, duration, in, out} in seconds; in/out -1 when unset.
    Q_PROPERTY(QVariantMap source READ source NOTIFY sourceChanged)
    // What an edit about to happen would do (M6, ripple scope): {active, moved: {clipId: -1|1},
    // removed: [clipIds], markers: count moved}. Filled by the preview calls, simulated on a
    // copy of the timeline with the very command the edit runs, so it cannot disagree with it.
    Q_PROPERTY(QVariantMap editScope READ editScope NOTIFY editScopeChanged)
    // Captions (ADR-0017): {id, start, duration, text} in seconds, sorted.
    Q_PROPERTY(QVariantList captions READ captions NOTIFY sequenceChanged)
    // The selected clip's transform at the playhead (keyframes evaluated): posX, posY, scale,
    // rotation; `keys` how many transform keys it has, `keyHere` whether one is at the playhead;
    // likewise opacity/opacityKeys/opacityKeyHere and gain/gainKeys/gainKeyHere (volume).
    Q_PROPERTY(QVariantMap motion READ motion NOTIFY motionChanged)
    // The selected clip's picture box at its committed transform, normalized to the canvas.
    Q_PROPERTY(QVariantMap selectedBox READ selectedBox NOTIFY motionChanged)
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

    // Also applies the preview quality from OMA_PREVIEW_SCALE: 1 (full), a fraction such as 0.5
    // (fixed), otherwise automatic (half resolution after sustained drops, M4).
    void setPreview(PreviewItem* preview);
    // Settings: "auto", "full", "half" or "quarter"; OMA_PREVIEW_SCALE, when set, wins.
    void setPreviewQuality(const QString& quality);
    // Settings: a recordings folder instead of Omarchy's ("" restores it).
    void setRecordingsFolderOverride(const QString& folder);
    // Automated runs: never play through the speakers.
    void setSilent(bool silent) { audio_.set_silent(silent); }
    // Desktop notifications (export finished); the smoke and audits turn them off.
    void setNotifications(bool on) { notifications_ = on; }
    // From Settings: "auto", "hardware" or "software" for the next exports.
    void setExportEncoder(const QString& choice) { export_encoder_ = choice; }
    void setOutputMuted(bool muted) { audio_.set_muted(muted); }
    [[nodiscard]] float takeAudioPeak() { return audio_.take_peak(); }
    [[nodiscard]] bool audioRunning() const { return audio_.running(); }
    [[nodiscard]] bool audioOnDevice() const { return audio_.on_device(); }
    // What the disk caches hold now, for Settings (bytes; directory scans, call off hot paths).
    [[nodiscard]] std::uint64_t cacheBytes() const { return thumbnail_cache_.size() + waveform_cache_.size(); }
    [[nodiscard]] QString cacheFolder() const;
    // ADR-0009 budgets: each store's limit, applied by an eviction pass on the job workers.
    void setCacheBudget(std::uint64_t bytes_per_store);
    // Empties both stores on the job workers, then emits cacheCleared.
    Q_INVOKABLE void clearCache();
    [[nodiscard]] std::int64_t audibleSample() const { return audio_.audible_sample(); }
    [[nodiscard]] std::int64_t audioUnderruns() const { return audio_.underruns(); }
    [[nodiscard]] std::int64_t droppedVideoFrames() const { return scheduler_.dropped(); }
    // How long the last playback (re)start held the UI thread, in milliseconds (M4 audit).
    [[nodiscard]] double lastRestartMs() const { return last_restart_ms_; }
    // M4 audit: how long each playback tick held the UI thread, and the time between ticks (ms).
    [[nodiscard]] std::pair<std::vector<double>, std::vector<double>> takeTickTimings() {
        return {std::exchange(tick_ms_, {}), std::exchange(tick_gap_ms_, {})};
    }

    [[nodiscard]] bool editing() const { return editing_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool failed() const { return failed_; }
    // The latest notice; once the GPU device is lost, its explanation stays when nothing newer shows.
    [[nodiscard]] QString notice() const;
    [[nodiscard]] QVariantList media() const;
    [[nodiscard]] int selectedMedia() const { return selected_media_; }
    [[nodiscard]] QVariantList clips() const;
    [[nodiscard]] QVariantList audioTracks() const;
    [[nodiscard]] QVariantList videoTracks() const;
    [[nodiscard]] QObject* storylineClips() { return &storyline_model_; }
    [[nodiscard]] QList<double> clipSpans() const;
    [[nodiscard]] bool storylineCompact() const { return storyline_compact_; }
    // The seconds the timeline view covers (with its margin): storylineClips holds only the
    // clips there, so an edit far from the view updates no delegate.
    Q_INVOKABLE void setStorylineView(double from, double to);
    // Selects the storyline clip at `seconds` (the compact strip has no delegates to click).
    Q_INVOKABLE void selectClipAt(double seconds);
    [[nodiscard]] QList<double> selectedSpan() const;
    // A clip's {start, duration} in seconds (for drawing); empty when it does not exist.
    Q_INVOKABLE QList<double> clipSpan(double id) const;
    // The selected library item at the playhead on a free lane above (video) or below (sound)
    // the storyline, connected to the storyline clip there (ADR-0014).
    Q_INVOKABLE void connectSelected();
    // Titles (ADR-0015): a three-second title at the playhead on a free lane above the
    // storyline, connected to the storyline clip there; and the selected title's text and style
    // (size as a fraction of the canvas height, colour as "#rrggbb", placement 0 lower third,
    // 1 centre, 2 top), one undoable edit.
    Q_INVOKABLE void addTitle();
    Q_INVOKABLE void setClipTitle(const QString& text, double size, const QString& color, int placement);
    // The selected lane clip: connect it to the storyline clip under its start, or disconnect it
    // (ADR-0014).
    Q_INVOKABLE void connectSelectedClip();
    Q_INVOKABLE void disconnectSelectedClip();
    [[nodiscard]] QObject* waveforms() { return &waveforms_; }
    [[nodiscard]] QVariantList filterPreviews() const { return filter_previews_; }
    [[nodiscard]] static QVariantList lookEffects();
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
    [[nodiscard]] QVariantList recordings() const { return recordings_; }
    [[nodiscard]] QString recordingsFolder() const { return recordings_folder_; }
    // Re-reads the recordings folder (also done when it changes on disk).
    Q_INVOKABLE void refreshRecordings();
    // Recent project files, kept in `settings_file` (the smoke run passes its own).
    void setRecentProjectsFile(const QString& settings_file);
    [[nodiscard]] QVariantList recentProjects() const;
    Q_INVOKABLE void forgetRecentProject(const QString& path);
    // Autosave (CLAUDE.md §14): while there are unsaved changes, a copy goes to the state folder
    // every kAutosaveSeconds; saving or discarding removes it. Copies left by a crash are listed
    // ({file, name, origin, when}) and can be restored (opened as unsaved changes to `origin`)
    // or discarded.
    static constexpr int kAutosaveSeconds = 30;
    [[nodiscard]] QVariantList recoveredProjects() const;
    Q_INVOKABLE void restoreRecovered(const QString& file);
    Q_INVOKABLE void discardRecovered(const QString& file);
    // Prior versions of a project file, newest first: {file, when}. Restoring opens one as
    // unsaved changes to that project.
    Q_INVOKABLE QVariantList projectVersions(const QString& project) const;
    Q_INVOKABLE void restoreVersion(const QString& file, const QString& project);
    // The window closes after the user chose to discard: the current autosave goes too.
    Q_INVOKABLE void discardAutosave();
    // Writes the autosave now if it is due (also what the timer calls).
    Q_INVOKABLE void autosave();
    // An import (or an opened project's re-import) is still running; saving waits for it.
    [[nodiscard]] bool importsPending() const { return importing(); }

    // Import. A file opened with the app goes straight to the end of the storyline (§3.1); one
    // imported from the dialog goes to the library, and to the storyline only when it is empty.
    // A project file (*.omamovie) opens as the project instead.
    Q_INVOKABLE void open(const QString& path);
    // How the app was opened with files (ui-design §3.1): a project file opens that project;
    // otherwise a new project gets the files on the storyline in the given order (imported one
    // after another, since imports finish in any order).
    Q_INVOKABLE void openFiles(const QStringList& paths);
    // Saves in the background (atomic replacement); an empty url saves to the current file.
    Q_INVOKABLE void saveProject(const QUrl& url);
    // Renders the sequence to an MP4 file (H.264 + AAC) on the job workers; cancellable.
    Q_INVOKABLE void exportMovie(const QUrl& url);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void clearExported() {
        exported_file_.clear();
        emit exportChanged();
    }
    [[nodiscard]] double exportProgress() const { return export_progress_; }
    [[nodiscard]] QString exportedFile() const { return exported_file_; }
    [[nodiscard]] const ExportStats& lastExportStats() const { return export_stats_; }
    // Points a missing library item (an opened project's moved file) at `url`, which must hold
    // the same kind of media at least as long as the clips use; then looks for the project's
    // other missing files in that folder.
    Q_INVOKABLE void relinkMedia(int index, const QUrl& url);
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
    // Clip timing (ADR-0013) on the selected clip, rippling the storyline: a constant speed
    // given as num/den (1/2 slow, 2 fast), a freeze of `seconds` at the playhead, or reverse.
    Q_INVOKABLE void setClipSpeed(int num, int den);
    Q_INVOKABLE void freezeFrame(double seconds);
    Q_INVOKABLE void reverseClip();
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
    // Effects (ADR-0016), by definition ID, each one command: add (after its stage) or remove,
    // bypass, a parameter value (adding the effect when absent), and a move within its stage.
    Q_INVOKABLE void setClipEffect(const QString& definition, bool on);
    Q_INVOKABLE void setClipEffectEnabled(const QString& definition, bool enabled);
    Q_INVOKABLE void setClipEffectParam(const QString& definition, const QString& name, double value);
    Q_INVOKABLE void moveClipEffect(const QString& definition, int step);
    // Blur (below 0) or sharpen (above 0), in [-1, 1]: the detail effect, removed at 0.
    Q_INVOKABLE void setClipSharpness(double sharpness);
    // Crop and framing: a Fit mode and the fractions cropped from each edge.
    Q_INVOKABLE void setClipFraming(int fit, double left, double top, double right, double bottom);
    // The selected clip's opacity in [0, 1] (composited over the layers below it). With opacity
    // keys (M8) it sets the key at the playhead, as the volume of setClipAudio does with volume
    // keys; the toggles add a key holding the value shown at the playhead, or remove the one there.
    Q_INVOKABLE void setClipOpacity(double opacity);
    Q_INVOKABLE void toggleOpacityKey();
    Q_INVOKABLE void toggleVolumeKey();
    // Direct manipulation in the viewer (UX-07): the selected clip's picture box for a candidate
    // position/scale, normalized to the canvas ({x, y, w, h}); a preview of a transform that only
    // the viewer shows; setClipTransform then commits the gesture as one edit.
    Q_INVOKABLE QVariantMap layerBox(double x, double y, double scale) const;
    [[nodiscard]] QVariantMap selectedBox() const {
        const QVariantMap m = motion();
        return m.isEmpty() ? QVariantMap{}
                           : layerBox(m.value("posX").toDouble(), m.value("posY").toDouble(), m.value("scale").toDouble());
    }
    Q_INVOKABLE void previewClipTransform(double x, double y, double scale, double rotation);
    // Crop handles in the viewer: the picture box and the canvas point of each edge's middle
    // ({x, y, w, h, left, top, right, bottom}, normalized) for a candidate crop; the crop
    // fraction an edge (0 left, 1 top, 2 right, 3 bottom) takes when dragged to a canvas point,
    // mapped back through the geometry of `from` (the crop when the drag began), so source
    // rotation and fit are honored; and a preview of a candidate crop. setClipFraming commits.
    Q_INVOKABLE QVariantMap cropBox(double left, double top, double right, double bottom) const;
    Q_INVOKABLE double cropEdgeAt(int edge, double x, double y, double left, double top, double right,
                                  double bottom) const;
    Q_INVOKABLE void previewClipFraming(double left, double top, double right, double bottom);
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
    Q_INVOKABLE void previewDelete(bool ripple);
    Q_INVOKABLE void previewTrim(double id, bool head, int frames);
    Q_INVOKABLE void clearEditScope();
    [[nodiscard]] QVariantMap editScope() const { return edit_scope_; }
    [[nodiscard]] QVariantList captions() const;
    // The caption text under `seconds` (the viewer's overlay), empty when none.
    Q_INVOKABLE QString captionAt(double seconds) const;
    // The caption under the playhead's id, or 0.
    Q_INVOKABLE double captionIdAt(double seconds) const;
    // Adds a caption at the playhead, three seconds or up to the next one; returns its id or 0.
    Q_INVOKABLE double addCaption();
    Q_INVOKABLE void setCaptionText(double id, const QString& text);
    Q_INVOKABLE void removeCaption(double id);
    // SRT or VTT (by content); replaces the captions as one undoable edit.
    Q_INVOKABLE void importCaptions(const QUrl& url);
    // .vtt by extension, else SRT.
    Q_INVOKABLE void exportCaptions(const QUrl& url);
    // The canvas proportion (ADR-0010), keeping its short side: 16:9, 9:16, 1:1, 4:5…; one undo
    // entry that also rescales every layer's position so it keeps its place in the frame.
    Q_INVOKABLE void setCanvasAspect(int w, int h);
    // Named speed ramps (M8, ADR-0013 segments) on the selected clip, using the same stretch of
    // media it shows now: 0 accelerate (½× to 1½×), 1 decelerate (1½× to ½×), 2 burst
    // (1× up to 2× and back, two thirds as long). One undoable edit; sound stays silent.
    Q_INVOKABLE void setSpeedRamp(int preset);
    Q_INVOKABLE void openSource(int index);
    // The source viewer's playhead (seek() is the sequence's and leaves the source viewer).
    Q_INVOKABLE void seekSource(double seconds);
    Q_INVOKABLE void closeSource();
    // Marks at the source playhead: in at its frame, out after it (the frame is kept).
    Q_INVOKABLE void markIn();
    Q_INVOKABLE void markOut();
    Q_INVOKABLE void clearMarks();
    [[nodiscard]] QVariantMap source() const;
    Q_INVOKABLE void toEnd();

signals:
    void editScopeChanged();
    void sourceChanged();
    void exportChanged();
    void cacheCleared();
    void viewChanged();
    void statusChanged();
    void libraryChanged();
    void recordingsChanged();
    void recentProjectsChanged();
    void recoveredChanged();
    // Another launch handed files to this instance; QML guards unsaved work, then openFiles().
    void openRequested(const QStringList& paths);
    void storylineViewChanged();
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
        bool missing = false; // an opened project's file that was not found: Locate… relinks it
        // Source viewer marks in sequence frames from the media's start, [in, out); session only:
        // a project saves the range a clip was given, not the marks.
        std::optional<std::int64_t> mark_in;
        std::optional<std::int64_t> mark_out;
    };

    // `known`: a library item of an opened project, imported again under its saved ID and
    // media range (the restored timeline references them).
    void importFile(const QString& path, bool append, std::optional<oma::project::MediaRef> known = std::nullopt);
    void applyProject(oma::project::Document doc, const QString& path);
    // A LUT of an opened project: parsed in the background into the tables under its saved ID.
    void loadLut(const QString& path, oma::timeline::LutId id);
    // What saving records: timeline, revision, library and LUT counts, and library edits that
    // keep the count (a relinked file). Equal to saved_ when nothing changed since the last save
    // or open.
    using Marker = std::tuple<bool, std::uint64_t, std::size_t, std::size_t, std::uint64_t>;
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
    // Tracks of `kind` other than the storyline, in timeline order (compositing bottom first).
    [[nodiscard]] std::vector<oma::timeline::TrackId> lanes(oma::timeline::TrackKind kind) const;
    [[nodiscard]] static std::string laneName(oma::timeline::TrackKind kind, std::size_t existing);
    bool placeOnLane(oma::timeline::TrackKind kind, int how, oma::timeline::ClipId id,
                     const oma::timeline::edit::ClipSource& clip, std::int64_t start,
                     std::optional<std::size_t> preferred, const char* name = nullptr);
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

    void rememberProject(const QString& path);
    [[nodiscard]] oma::project::Document document() const;
    [[nodiscard]] bool importing() const;

    QTimer autosave_timer_;
    QString autosave_file_;     // this session's autosave, once written
    Marker autosaved_{false, 0, 0, 0, 0};
    std::uint64_t library_revision_ = 0; // only grows
    // Folders the user pointed at while relinking: later missing files are looked for there too.
    std::vector<std::filesystem::path> relink_roots_;
    oma::JobHandle autosave_job_;
    QString restoring_;         // an autosave being opened by restoreRecovered
    bool restoring_version_ = false; // ...or a prior version (restoreVersion), which is kept
    QString restoring_origin_;
    void importNextQueued(unsigned generation);

    std::unique_ptr<RecentProjects> recent_;
    QStringList queued_files_; // openFiles() media still to import, in order

    PreviewItem* preview_ = nullptr;
    bool editing_ = false;
    QString status_;
    bool failed_ = false;
    QString notice_;
    QTimer notice_timer_;
    bool device_lost_ = false;

    std::vector<LibraryItem> library_;
    int selected_media_ = -1;
    std::uint64_t next_media_ = 1;
    std::uint64_t next_lut_ = 1;
    std::unordered_map<std::uint64_t, QString> lut_paths_; // LUT ID -> its .cube file
    QString project_path_;
    Marker saved_{false, 0, 0, 0, 0};
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
    mutable std::optional<QVariantList> clips_cache_;
    ClipListModel storyline_model_;
    double view_from_ = 0.0;
    double view_to_ = std::numeric_limits<double>::infinity();
    bool storyline_compact_ = false;
    void syncStoryline();
    QVariantList recordings_;
    QString recordings_folder_;
    QFileSystemWatcher recordings_watcher_;
    QTimer recordings_settle_; // re-reads while a capture is still being written
    // Whether a finished recording has sound, by "path|mtime"; probed on the job workers.
    QHash<QString, bool> recording_audio_;
    QSet<QString> recording_probes_;
    double last_restart_ms_ = 0.0;
    bool previewing_ = false; // the snapshot carries an uncommitted gesture
    // Source pixels → canvas pixels for the selected clip with this crop and transform. The
    // viewer draws the box unrotated and turns it about its center (the compositor's pivot).
    struct Framing {
        oma::compositor::Affine matrix;
        double width = 0;
        double height = 0;
    };
    [[nodiscard]] std::optional<Framing> framing(const oma::timeline::Crop& crop, double x, double y, double scale,
                                                 double rotation = 0.0) const;
    void previewSelectedVideo(const oma::timeline::VideoProperties& video);
    [[nodiscard]] std::optional<oma::timeline::VideoProperties> transformedVideo(double x, double y, double scale,
                                                                                double rotation) const;
    enum class PreviewQuality : std::uint8_t { Full, Fixed, Auto };
    PreviewQuality preview_quality_ = PreviewQuality::Auto;
    bool preview_quality_from_env_ = false;
    QString recordings_override_;
    std::int64_t adapt_shown_ = 0;
    std::int64_t adapt_dropped_base_ = 0;
    void adaptPreview();
    std::vector<double> tick_ms_;
    std::vector<double> tick_gap_ms_;
    QElapsedTimer tick_clock_;
    QTimer tick_;
    VideoScheduler scheduler_;
    // Immutable views shared with the pipelines: the timeline as of the last edit and the
    // library's files. Replaced, never mutated.
    std::shared_ptr<const oma::timeline::Timeline> snapshot_;
    // Source viewer: the item shown, its playhead (frames) and a one-clip timeline that renders
    // it through the viewer's own path. -1: the sequence is shown.
    int source_index_ = -1;
    QVariantMap edit_scope_;
    // The commands behind Delete/Lift and trimming, shared by the edits and their previews.
    [[nodiscard]] std::unique_ptr<oma::timeline::Command> deleteCommand(bool ripple) const;
    [[nodiscard]] std::unique_ptr<oma::timeline::Command> trimCommand(oma::timeline::ClipId clip, bool head, int frames) const;
    void previewCommand(std::unique_ptr<oma::timeline::Command> command);
    std::int64_t source_frame_ = 0;
    std::int64_t source_frames_ = 0;
    std::shared_ptr<const oma::timeline::Timeline> source_timeline_;
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
    oma::JobHandle export_job_;
    double export_progress_ = -1; // UI thread
    QString exported_file_;
    ExportStats export_stats_; // of the last finished export (UI thread)
    std::atomic<std::int64_t> export_done_{0}; // written by the export job, read by a timer
    std::int64_t export_total_ = 0;
    QTimer export_timer_;

    QTemporaryDir thumbnails_;
    // Disk caches (ADR-0009), read and filled by the import and waveform workers: declared before
    // them so they outlive every job.
    oma::DiskCache thumbnail_cache_;
    oma::DiskCache waveform_cache_;
    // Accessed only by the import job worker; its lifetime extends past worker shutdown.
    std::unique_ptr<oma::media::MediaImporter> importer_;
    FrameSource frames_;      // job worker only
    AudioPlayer audio_;
    WaveformStore waveforms_;
    bool warned_silent_ = false;
    bool notifications_ = true;
    QString export_encoder_ = QStringLiteral("auto");
    QVariantList filter_previews_;
    unsigned previews_ = 0; // filter preview requests, so a stale result is dropped
    oma::JobHandle previews_job_;
    oma::JobPool workers_{1}; // destroyed late: no job outlives what it touches
    // Exports run for minutes; on the shared worker they held up saving, autosave, imports and
    // the software viewer until they ended. An export owns its decoders and device, so it needs
    // nothing from `workers_`. Declared last: destroyed (and drained) first.
    oma::JobPool export_pool_{1};
};
