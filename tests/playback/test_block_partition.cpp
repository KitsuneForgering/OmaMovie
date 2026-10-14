// M10 audio boundaries: the mix must not depend on how the device or the exporter cuts it into
// blocks. Every case is rendered in fixed 1024-sample blocks and again in seeded irregular blocks
// (1 sample to 4096, and frame-sized 1600) in one TimelineAudio, and must match sample for sample.
#include "speed_test.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

// `frames` from `first` in blocks drawn from a small LCG (seed 0: frame-sized 1600 blocks).
std::vector<float> render_irregular(const tl::Timeline& timeline, std::int64_t first,
                                    std::int64_t frames, std::uint32_t seed) {
    oma::playback::TimelineAudio audio(timeline, paths(), rate(), kChannels);
    std::vector<float> out(static_cast<std::size_t>(frames * kChannels));
    std::uint32_t state = seed;
    for (std::int64_t at = 0; at < frames;) {
        state = (state * 1664525U) + 1013904223U;
        const std::int64_t size = seed == 0 ? 1600 : 1 + static_cast<std::int64_t>(state >> 20);
        const std::int64_t n = std::min(size, frames - at);
        const auto span = std::span<float>(out).subspan(static_cast<std::size_t>(at * kChannels),
                                                        static_cast<std::size_t>(n * kChannels));
        if (!audio.render(span, first + at))
            return {};
        at += n;
    }
    return out;
}

bool same_for_every_partition(const tl::Timeline& timeline, std::int64_t frames) {
    const auto fixed = render(timeline, 0, frames);
    if (fixed.empty() || peak(fixed, 0, frames) == 0.0F)
        return false;
    for (const std::uint32_t seed : {0U, 1U, 7U, 2026U}) {
        if (render_irregular(timeline, 0, frames, seed) != fixed)
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

} // namespace

void run_block_partition_tests() {
    describe("playback::TimelineAudio block partitions", {
        it("mixes keys, a dissolve, EQ and 2x speed the same in any block sizes", {
            if (!have_fixture("tone_44100.wav"))
                return;
            expect(same_for_every_partition(edited(), 72000)).toBeTruthy();
        });
        it("reduces noise the same in any block sizes", {
            if (!have_fixture("noisy_tone.wav"))
                return;
            expect(same_for_every_partition(denoised(), 144000)).toBeTruthy();
        });
    });
}
