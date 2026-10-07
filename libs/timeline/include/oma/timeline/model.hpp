#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/ids.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
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

// A 3D color lookup table the project uses (ADR-0012). Its entries live outside the model (the
// app loads the file); clips reference it by ID, like media.
struct LutInfo {
    LutId id;
    std::string name;
};

// The most segments one time map holds: ramp presets use a handful; the cap bounds what an
// untrusted project file can make the model allocate and evaluate (CLAUDE.md §18).
inline constexpr std::size_t kMaxTimeSegments = 256;

// One piece of a segmented time map (ADR-0013). `length` is in sequence ticks.
struct TimeSegment {
    enum class Kind : std::uint8_t {
        Linear, // constant speed `from` (negative plays backwards, never zero)
        Freeze, // the media position held
        Ramp,   // speed changing linearly from `from` to `to`; never changes direction
    };
    Kind kind = Kind::Linear;
    std::int64_t length = 0;
    Rational from = Rational::literal(1, 1);
    Rational to = Rational::literal(1, 1);

    friend bool operator==(const TimeSegment&, const TimeSegment&) noexcept = default;
};

// Maps time local to a clip (sequence timebase) to an offset in media time (ADR-0002,
// ADR-0013): either one constant speed for the whole clip, or segments covering the clip
// exactly (freeze, reverse, ramps). Offsets are exact; rounding to a media PTS happens only in
// `evaluate`.
class TimeMap {
public:
    TimeMap() noexcept = default;

    // Speed > 0: 1 is normal, 1/2 is half speed (clip twice as long), 2 is double speed.
    [[nodiscard]] static Result<TimeMap> constant(Rational speed);
    // Segments in order; each one starts where the previous one left the media.
    [[nodiscard]] static Result<TimeMap> segmented(std::vector<TimeSegment> segments);

    [[nodiscard]] bool is_constant() const noexcept { return segments_.empty(); }
    // The constant speed; meaningful only when is_constant().
    [[nodiscard]] Rational speed() const noexcept { return speed_; }
    [[nodiscard]] std::span<const TimeSegment> segments() const noexcept { return segments_; }
    // Ticks the segments cover (0 for a constant map, which fits any duration).
    [[nodiscard]] std::int64_t length() const noexcept;
    // Whether the media position decreases at local tick `t` (a reverse segment).
    [[nodiscard]] bool backward(std::int64_t t) const noexcept;

    // Media displacement after `local_ticks` of sequence time, exact (no rounding).
    [[nodiscard]] Result<RationalTime> media_offset(std::int64_t local_ticks,
                                                    Rational sequence_timebase) const;
    // The smallest and largest displacement over [0, duration] (the media a clip spans).
    [[nodiscard]] Result<std::pair<RationalTime, RationalTime>>
    extent(std::int64_t duration, Rational sequence_timebase) const;
    // The map of local ticks [from, to), starting at displacement 0 (split and trims).
    [[nodiscard]] Result<TimeMap> slice(std::int64_t from, std::int64_t to) const;
    // Grown by `before` ticks at the start and `after` at the end (outward trims): the edge
    // segment keeps going at its edge speed (a ramp gets a linear or frozen extension).
    // A constant map is unchanged.
    [[nodiscard]] Result<TimeMap> extended(std::int64_t before, std::int64_t after) const;
    // The same motion as segments covering `duration` (a constant map becomes one linear one).
    [[nodiscard]] Result<TimeMap> as_segments(std::int64_t duration) const;
    // Played backwards over `duration`: segment order reversed and speeds negated. Starts where
    // this map ends, so the caller moves source_in to media_offset(duration).
    [[nodiscard]] Result<TimeMap> reversed(std::int64_t duration) const;

    friend bool operator==(const TimeMap&, const TimeMap&) noexcept = default;

private:
    explicit TimeMap(Rational speed) noexcept : speed_(speed) {}

    Rational speed_ = Rational::literal(1, 1);
    std::vector<TimeSegment> segments_;
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

// Color adjustments (ui-design §6, Color); 0 leaves the picture unchanged.
struct ColorAdjust {
    double exposure = 0.0;    // stops, in [-4, 4]
    double contrast = 0.0;    // in [-1, 1]
    double saturation = 0.0;  // in [-1, 1]: -1 black and white
    double temperature = 0.0; // in [-1, 1]: cooler to warmer

    friend bool operator==(const ColorAdjust&, const ColorAdjust&) noexcept = default;
};

// The looks a look effect applies (effects.hpp); mirrors compositor::FilterKind.
enum class FilterKind : std::uint8_t {
    None,
    BlackAndWhite,
    Sepia,
    Vintage,
    Cool,
    Warm,
    Vignette,
};

// One video effect of a clip (ADR-0016): a definition named by a stable ID (effects.hpp), a
// bypass flag and parameter values. A definition this build does not know is kept verbatim so
// saving writes it back; it is not rendered.
inline constexpr std::size_t kMaxEffects = 16;
inline constexpr std::size_t kMaxEffectParams = 16;
inline constexpr std::size_t kMaxEffectNameBytes = 64;

struct EffectParam {
    std::string name;
    double value = 0.0;

    friend bool operator==(const EffectParam&, const EffectParam&) = default;
};

struct Effect {
    std::string definition;
    bool enabled = true;
    std::vector<EffectParam> params; // unique names; a missing one takes its default

    friend bool operator==(const Effect&, const Effect&) = default;
};

// Color grading (ADR-0012), applied after the color adjustments and the filter, on the clip's
// code values. Mirrors compositor::Grade; the app maps one onto the other.
//
// ASC CDL v1.2: per channel clamp(in * slope + offset)^power, then saturation. The UI shows
// lift/gamma/gain, which map onto it exactly.
struct Cdl {
    std::array<double, 3> slope{1.0, 1.0, 1.0};  // in [0, 4]
    std::array<double, 3> offset{0.0, 0.0, 0.0}; // in [-1, 1]
    std::array<double, 3> power{1.0, 1.0, 1.0};  // in [0.1, 4]
    double saturation = 1.0;                     // in [0, 4]

    friend bool operator==(const Cdl&, const Cdl&) noexcept = default;
};

struct CurvePoint {
    double x = 0.0; // in [0, 1]
    double y = 0.0; // in [0, 1]

    friend bool operator==(const CurvePoint&, const CurvePoint&) noexcept = default;
};

inline constexpr std::size_t kMaxCurvePoints = 16;

// Each empty (identity) or 2 to kMaxCurvePoints points with strictly increasing x.
struct Curves {
    std::vector<CurvePoint> master;
    std::vector<CurvePoint> red;
    std::vector<CurvePoint> green;
    std::vector<CurvePoint> blue;

    friend bool operator==(const Curves&, const Curves&) noexcept = default;
};

struct ColorGrade {
    Cdl cdl;
    Curves curves;
    LutId lut;               // none when invalid; otherwise a LutInfo of the timeline
    double lut_amount = 1.0; // in [0, 1]

    friend bool operator==(const ColorGrade&, const ColorGrade&) noexcept = default;
};

// How a keyframed value moves from a key to the next one.
enum class Interpolation : std::uint8_t {
    Hold,   // stays until the next key
    Linear, // constant rate
    Ease,   // slow out and in (smoothstep)
};

inline constexpr std::size_t kMaxKeys = 256;

// The transform at one instant of the clip's media (Ken Burns, M8 keyframes). Keys sit in source
// time, comparable to Clip::source_in, so split, trims, slip and speed changes keep the motion
// on the same picture without touching the keys; a key outside the clip's range still shapes
// the motion inside it.
struct TransformKey {
    RationalTime at;
    Transform value;
    Interpolation interpolation = Interpolation::Linear; // toward the next key

    friend bool operator==(const TransformKey&, const TransformKey&) noexcept = default;
};

// A title clip's text (ADR-0015): drawn by the app over a transparent background at the canvas
// size, then composited like any picture.
enum class TitlePlacement : std::uint8_t { LowerThird, Center, Top };

inline constexpr std::size_t kMaxTitleBytes = 1000;

struct Title {
    std::string text;   // UTF-8, line breaks allowed, at most kMaxTitleBytes
    std::string font;   // family name; empty: the UI font
    double size = 0.08; // text height as a fraction of the canvas height, (0, 0.5]
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F}; // straight sRGB RGBA in [0, 1]
    TitlePlacement placement = TitlePlacement::LowerThird;

    friend bool operator==(const Title&, const Title&) = default;
};

struct VideoProperties {
    Fit fit = Fit::Fit;
    Crop crop;
    Transform transform; // used when transform_keys is empty
    // At most kMaxKeys, strictly increasing `at`; timeline::transform_at evaluates them.
    std::vector<TransformKey> transform_keys;
    float opacity = 1.0F;
    BlendMode blend = BlendMode::Normal;
    ColorAdjust color;
    // At most kMaxEffects, each definition at most once; rendered by stage, in this order
    // within a stage (ADR-0016).
    std::vector<Effect> effects;
    ColorGrade grade;

    friend bool operator==(const VideoProperties&, const VideoProperties&) = default;
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

enum class TransitionKind : std::uint8_t {
    Dissolve,   // the incoming clip fades in over the outgoing one
    DipToBlack, // out to black, then in from black
    Wipe,       // the incoming clip is revealed from left to right
};

// A transition into a clip from the clip that ends exactly where it starts, centered on that cut
// (ui-design §7.2). Both clips play past the cut for half of it, so it needs media to spare on
// both sides; with less, it gets shorter, and without a neighbour it is a plain cut. Sound
// crossfades over the same span. Timeline duration does not change.
struct Transition {
    TransitionKind kind = TransitionKind::Dissolve;
    RationalTime duration; // sequence timebase, > 0

    friend bool operator==(const Transition&, const Transition&) noexcept = default;
};

// A clip places a range of a media item on a track. Non-destructive: it only references media.
// A connected clip's attachment (ADR-0014): its first instant is where the primary clip shows
// `source`, so it follows the primary's content through moves, ripples, trims and slips.
struct Anchor {
    ClipId primary;
    RationalTime source; // a media position of the primary, exact

    friend bool operator==(const Anchor&, const Anchor&) noexcept = default;
};

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
    std::optional<Transition> transition_in;
    std::optional<Anchor> anchor; // connected to a clip on another track (ADR-0014)
    // A generated title instead of media (ADR-0015): `media` is then 0 and the map 1×.
    std::optional<Title> title;

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

    // A timeline rebuilt from saved state (libs/project): the parts as they were, checked by
    // validate(); new IDs continue after the largest one used. Not an edit: the caller starts a
    // fresh history (Editor::clear_history is implicit in a new Editor).
    [[nodiscard]] static Result<Timeline>
    restore(FrameRate rate, Rational timebase, std::vector<Track> tracks,
            std::vector<Marker> markers, std::vector<MediaInfo> media, std::vector<LutInfo> luts);

    [[nodiscard]] FrameRate frame_rate() const noexcept { return rate_; }
    [[nodiscard]] Rational timebase() const noexcept { return timebase_; }

    [[nodiscard]] std::span<const Track> tracks() const noexcept { return tracks_; }
    [[nodiscard]] std::span<const Marker> markers() const noexcept { return markers_; }
    [[nodiscard]] std::span<const MediaInfo> media() const noexcept { return media_; }
    [[nodiscard]] std::span<const LutInfo> luts() const noexcept { return luts_; }

    [[nodiscard]] const Track* find_track(TrackId id) const noexcept;
    [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
    [[nodiscard]] const Track* track_of(ClipId id) const noexcept;
    [[nodiscard]] const MediaInfo* find_media(MediaId id) const noexcept;
    [[nodiscard]] const LutInfo* find_lut(LutId id) const noexcept;
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
    std::vector<LutInfo> luts_;
    // IDs only grow, even across undo, so an ID is never reused for another object.
    std::uint64_t next_id_ = 1;
};

} // namespace oma::timeline
