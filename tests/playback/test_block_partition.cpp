// M10 audio boundaries: the mix must not depend on how the device or the exporter cuts it into
// blocks. Every case is rendered in fixed 1024-sample blocks and again in seeded irregular blocks
// (1 to 4096 samples) and in the exporter's per-frame blocks at 23.976/29.97/30/59.94 fps, in one
// TimelineAudio each, and must match sample for sample.
#include "speed_test.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

// `frames` from `first`, each block as long as `next(block index)` says.
template <typename Next>
std::vector<float> render_in(const tl::Timeline& timeline, std::int64_t first, std::int64_t frames,
                             Next next) {
    oma::playback::TimelineAudio audio(timeline, paths(), rate(), kChannels);
    std::vector<float> out(static_cast<std::size_t>(frames * kChannels));
    std::int64_t block = 0;
    for (std::int64_t at = 0; at < frames; ++block) {
        const std::int64_t n = std::min(next(block), frames - at);
        if (n <= 0)
            return {};
        const auto span = std::span<float>(out).subspan(static_cast<std::size_t>(at * kChannels),
                                                        static_cast<std::size_t>(n * kChannels));
        if (!audio.render(span, first + at))
            return {};
        at += n;
    }
    return out;
}

// Seeded irregular blocks of 1 to 4096 samples, from a small LCG.
auto irregular(std::uint32_t seed) {
    return [state = seed](std::int64_t) mutable {
        state = (state * 1664525U) + 1013904223U;
        return 1 + static_cast<std::int64_t>(state >> 20);
    };
}

// The exporter's blocks: the samples from frame k's start to frame k + 1's, floored at 48 kHz,
// so NTSC rates alternate (1601/1602 at 29.97).
auto per_frame(oma::FrameRate fps) {
    const auto tb = q(1, 48000);
    return [fps, tb](std::int64_t k) {
        const auto at = [&](std::int64_t f) {
            return fps.frame_to_time(f).rescaled(tb, oma::Rounding::Floor).value().value();
        };
        return at(k + 1) - at(k);
    };
}

bool same_for_every_partition(const tl::Timeline& timeline, std::int64_t frames) {
    const auto fixed = render(timeline, 0, frames);
    if (fixed.empty() || peak(fixed, 0, frames) == 0.0F)
        return false;
    for (const std::uint32_t seed : {1U, 7U, 2026U}) {
        if (render_in(timeline, 0, frames, irregular(seed)) != fixed)
            return false;
    }
    namespace fr = oma::frame_rates;
    for (const oma::FrameRate fps : {fr::k23_976, fr::k29_97, fr::k30, fr::k59_94}) {
        if (render_in(timeline, 0, frames, per_frame(fps)) != fixed)
            return false;
    }
    return true;
}

// Linear volume keys, a cut with an EQ'd clip dissolving in, then a clip at 2x.
tl::Timeline edited() {
    Sequence seq = make_sequence();
    tl::AudioProperties keyed;
    keyed.gain_keys = {
        tl::ScalarKey{.at = s(0), .value = 1.0, .interpolation = tl::Interpolation::Linear},
        tl::ScalarKey{.at = s(24000), .value = 0.2, .interpolation = tl::Interpolation::Linear}};
    (void)place(seq, 0, 0, 24000, keyed);
    tl::AudioProperties eq;
    eq.eq.mid_db = -12.0F;
    const tl::ClipId b = place(seq, 24000, 44100, 24000, eq);
    (void)seq.editor.execute(tl::edit::set_transition(
        b, tl::Transition{.kind = tl::TransitionKind::Dissolve, .duration = s(9600)}));
    (void)place_at_speed(seq, 48000, 0, 24000, q(2, 1));
    return seq.editor.timeline();
}

tl::Timeline denoised() {
    Sequence seq = make_sequence();
    tl::AudioProperties a;
    a.noise.amount = 1.0F;
    a.noise.floor_db = -38.7F;
    (void)place_noisy(seq, a);
    return seq.editor.timeline();
}

// A sequence timebase of whole frames (1/30, as a project without one is read) holds no 48 kHz
// sample: rendering refuses with silence instead of playing one tick per sample (a 1 s clip
// heard for 30 samples, reported as success before).
bool refuses_a_timebase_without_samples() {
    tl::Editor ed(tl::Timeline::create(oma::frame_rates::k30, q(1, 30)).value());
    (void)ed.add_media(
        tl::MediaInfo{.id = kTone,
                      .start = oma::RationalTime::make(0, q(1, 44100)).value(),
                      .duration = oma::RationalTime::make(88200, q(1, 44100)).value(),
                      .has_video = false,
                      .has_audio = true,
                      .still = false});
    const tl::TrackId track = ed.new_track_id();
    (void)ed.execute(tl::edit::add_track(track, tl::TrackKind::Audio, "A1"));
    const auto frame = [](std::int64_t n) {
        return oma::RationalTime::make(n, q(1, 30)).value();
    };
    const tl::edit::ClipSource source{.media = kTone,
                                      .source_in = frame(0),
                                      .duration = frame(30),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}};
    if (!ed.execute(tl::edit::overwrite(track, ed.new_clip_id(), frame(0), source)))
        return false;
    oma::playback::TimelineAudio audio(ed.timeline(), paths(), rate(), kChannels);
    std::vector<float> out(static_cast<std::size_t>(4800 * kChannels), 1.0F);
    const auto r = audio.render(out, 0);
    return !r && r.error().code() == oma::ErrorCode::Unsupported && peak(out, 0, 4800) == 0.0F;
}
} // namespace

void run_block_partition_tests() {
    describe("playback::TimelineAudio block partitions", {
        it("mixes keys, a dissolve, EQ and 2x speed the same in any block sizes", {
            if (!have_fixture("tone_44100.wav"))
                return;
            expect(same_for_every_partition(edited(), 72000)).toBeTruthy();
        });
        it("refuses a sequence timebase that holds no whole samples", {
            if (!have_fixture("tone_44100.wav"))
                return;
            expect(refuses_a_timebase_without_samples()).toBeTruthy();
        });
        it("reduces noise the same in any block sizes", {
            if (!have_fixture("noisy_tone.wav"))
                return;
            expect(same_for_every_partition(denoised(), 144000)).toBeTruthy();
        });
    });
}
