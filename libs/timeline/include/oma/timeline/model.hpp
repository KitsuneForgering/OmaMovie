#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/ids.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The timeline model (CLAUDE.md §10): plain C++, no Qt, GPU or FFmpeg. Read access is public;
// changes go only through commands executed by an Editor (oma/timeline/editor.hpp).

namespace oma::timeline {

namespace detail {
class Mutation;
} // namespace detail

// What the timeline knows about a media item. The library/probe layer fills it; the timeline
// only needs enough to validate references and source ranges.
struct MediaInfo {
    MediaId id;
    // Available source range in media time, [start, start + duration). Ignored for stills.
    RationalTime start;
    RationalTime duration;
    bool has_video = false;
    bool has_audio = false;
    bool still = false; // an image: any source range is valid
};

// Maps time local to a clip (sequence timebase) to an offset in media time (ADR-0002).
// v0.1 creates constant-speed maps only. Reverse, freeze and ramps need the complementary
// interpolation/domain ADR before they exist (ADR-0002, M5 note); the type is already the one
// the model and the project format use, so adding them needs no structural migration.
class TimeMap {
public:
    TimeMap() noexcept = default;

    // Speed > 0: 1 is normal, 1/2 is half speed (clip twice as long), 2 is double speed.
    [[nodiscard]] static Result<TimeMap> constant(Rational speed);

    [[nodiscard]] Rational speed() const noexcept { return speed_; }

    // Media displacement covered by `local_ticks` of sequence time, exact (no rounding): a time
    // in the unit `sequence_timebase * speed`.
    [[nodiscard]] Result<RationalTime> media_offset(std::int64_t local_ticks,
                                                    Rational sequence_timebase) const;

    friend bool operator==(const TimeMap&, const TimeMap&) noexcept = default;

private:
    explicit TimeMap(Rational speed) noexcept : speed_(speed) {}

    Rational speed_ = Rational::literal(1, 1);
};

enum class TrackKind : std::uint8_t {
    Video,
    Audio,
    Caption, // in the model from the start; caption clips arrive with v0.2
};

// Visual parameters of a clip. Spatial values are not time, so doubles are fine here; their
// meaning matches the compositor's render graph, which the playback layer builds from them.
enum class Fit : std::uint8_t {
    Fit,
    Fill,
    Stretch,
    Native,
};

enum class BlendMode : std::uint8_t {
    Normal,
    Add,
    Multiply,
    Screen,
};

struct Crop {
    double left = 0.0; // fractions of the source removed from each edge, in [0, 1)
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;

    friend bool operator==(const Crop&, const Crop&) noexcept = default;
};

struct Transform {
    double offset_x = 0.0; // output pixels from the output center
    double offset_y = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double rotation = 0.0; // degrees, clockwise

    friend bool operator==(const Transform&, const Transform&) noexcept = default;
};

struct VideoProperties {
    Fit fit = Fit::Fit;
    Crop crop;
    Transform transform;
    float opacity = 1.0F;
    BlendMode blend = BlendMode::Normal;

    friend bool operator==(const VideoProperties&, const VideoProperties&) noexcept = default;
};

// A three-band equalizer, gains in dB within ±24 (0 is flat).
struct Equalizer {
    float low_db = 0.0F;
    float mid_db = 0.0F;
    float high_db = 0.0F;

    friend bool operator==(const Equalizer&, const Equalizer&) noexcept = default;
};

// Reduction of steady background noise (hiss, hum, fans).
struct NoiseReduction {
    float amount = 0.0F;     // in [0, 1]; 0 is off
    float floor_db = -50.0F; // the noise level it removes, dBFS RMS, measured from the clip

    friend bool operator==(const NoiseReduction&, const NoiseReduction&) noexcept = default;
};

struct AudioProperties {
    float gain = 1.0F; // linear
    bool muted = false;
    // Fade lengths in timeline time; together they fit in the clip.
    RationalTime fade_in;
    RationalTime fade_out;
    Equalizer eq;
    NoiseReduction noise;

    friend bool operator==(const AudioProperties&, const AudioProperties&) noexcept = default;
};

// A clip places a range of a media item on a track. Non-destructive: it only references media.
struct Clip {
    ClipId id;
    MediaId media;
    RationalTime start;    // sequence timebase
    RationalTime duration; // sequence timebase, > 0
    // Media time shown at the clip's first instant. Exact, in any timebase: trims move it by
    // exact amounts, and rounding to a media PTS happens only on evaluation.
    RationalTime source_in;
    TimeMap time_map;
    VideoProperties video;
    AudioProperties audio;
    // On a video track: its sound was detached into an audio clip of its own (edit::detach_audio),
    // so this clip plays no sound. Always false on other tracks.
    bool audio_detached = false;

    // Exact end: start + duration.
    [[nodiscard]] std::int64_t start_ticks() const noexcept { return start.value(); }
    [[nodiscard]] std::int64_t end_ticks() const noexcept {
        return start.value() + duration.value();
    }
};

struct Track {
    TrackId id;
    TrackKind kind = TrackKind::Video;
    std::string name;
    bool muted = false;      // audio of this track is not mixed
    bool hidden = false;     // video of this track is not composited
    std::vector<Clip> clips; // sorted by start, never overlapping
};

struct Marker {
    MarkerId id;
    RationalTime time; // sequence timebase
    std::string name;
};

class Timeline {
public:
    // `timebase` is the sequence tick: every timeline position is an integer multiple of it.
    // It must hold both the frame grid and audio samples (audio edits are not quantized to
    // frames, CLAUDE.md §6); default_timebase() picks one.
    [[nodiscard]] static Result<Timeline> create(FrameRate rate, Rational timebase);
    [[nodiscard]] static Result<Timeline> create(FrameRate rate, SampleRate audio_rate);

    // 1 / lcm(frame-rate numerator, sample rate): 30000/1001 fps and 48 kHz give 1/240000,
    // where one frame is 8008 ticks and one sample 5.
    [[nodiscard]] static Result<Rational> default_timebase(FrameRate rate, SampleRate audio_rate);

    [[nodiscard]] FrameRate frame_rate() const noexcept { return rate_; }
    [[nodiscard]] Rational timebase() const noexcept { return timebase_; }

    [[nodiscard]] std::span<const Track> tracks() const noexcept { return tracks_; }
    [[nodiscard]] std::span<const Marker> markers() const noexcept { return markers_; }
    [[nodiscard]] std::span<const MediaInfo> media() const noexcept { return media_; }

    [[nodiscard]] const Track* find_track(TrackId id) const noexcept;
    [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
    [[nodiscard]] const Track* track_of(ClipId id) const noexcept;
    [[nodiscard]] const MediaInfo* find_media(MediaId id) const noexcept;
    [[nodiscard]] const Marker* find_marker(MarkerId id) const noexcept;
    // The clip covering `ticks` on a track, if any.
    [[nodiscard]] const Clip* clip_at(TrackId track, std::int64_t ticks) const noexcept;

    // A time in sequence ticks. Times off the sequence grid are rejected, never rounded:
    // edits land exactly where they were asked to.
    [[nodiscard]] Result<std::int64_t> to_ticks(const RationalTime& t) const;
    [[nodiscard]] RationalTime at(std::int64_t ticks) const noexcept;

    // End of the last clip on any track.
    [[nodiscard]] RationalTime duration() const noexcept;

    // Checks every invariant: unique IDs, sorted non-overlapping clips with positive
    // durations on the sequence grid, existing media of a kind the track accepts, source
    // ranges inside the media, fades that fit, valid visual parameters, markers on the grid.
    [[nodiscard]] Result<void> validate() const;

private:
    Timeline(FrameRate rate, Rational timebase) noexcept : rate_(rate), timebase_(timebase) {}

    friend class detail::Mutation;

    FrameRate rate_;
    Rational timebase_;
    std::vector<Track> tracks_; // video tracks bottom first, then any order for audio
    std::vector<Marker> markers_;
    std::vector<MediaInfo> media_;
    // IDs only grow, even across undo, so an ID is never reused for another object.
    std::uint64_t next_id_ = 1;
};

} // namespace oma::timeline
