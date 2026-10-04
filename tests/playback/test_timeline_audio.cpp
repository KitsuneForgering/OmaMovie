#include "playback_test.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

// The fixture's mono tone peaks at 1/8. The decoder's upmix to stereo spreads it at -3 dB per
// channel, and resampling to 48 kHz may overshoot slightly.
bool near_tone(float p, float gain) {
    return std::abs(p - (0.125F * 0.70710678F * gain)) < 0.005F;
}

tl::AudioProperties with_gain(float gain) {
    tl::AudioProperties a;
    a.gain = gain;
    return a;
}

tl::AudioProperties muted() {
    tl::AudioProperties a;
    a.muted = true;
    return a;
}

} // namespace

void run_timeline_audio_tests() {
    describe("playback::TimelineAudio", {
        it("plays a clip's audio only inside the clip", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            Sequence seq = make_sequence();
            expect(place(seq, 4800, 0, 24000).valid()).toBeTruthy();
            const auto out = render(seq.editor.timeline(), 0, 48000);
            expect(out.empty()).toBeFalsy();
            expect(peak(out, 0, 4800)).toEqual(0.0F);
            expect(near_tone(peak(out, 4800, 28800), 1.0F)).toBeTruthy();
            expect(peak(out, 28800, 48000)).toEqual(0.0F);
        });

        it("applies the clip's gain and mute", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            Sequence seq = make_sequence();
            expect(place(seq, 0, 0, 24000, with_gain(0.5F)).valid()).toBeTruthy();
            expect(place(seq, 24000, 0, 24000, muted()).valid()).toBeTruthy();
            const auto out = render(seq.editor.timeline(), 0, 48000);
            expect(near_tone(peak(out, 0, 24000), 0.5F)).toBeTruthy();
            expect(peak(out, 24000, 48000)).toEqual(0.0F);
        });

        it("plays a split clip exactly like the unsplit clip", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            Sequence whole = make_sequence();
            expect(place(whole, 0, 0, 48000).valid()).toBeTruthy();
            Sequence split = make_sequence();
            // Sequence sample 20000 is media sample 18375 at 44.1 kHz: the halves are contiguous.
            expect(place(split, 0, 0, 20000).valid()).toBeTruthy();
            expect(place(split, 20000, 18375, 28000).valid()).toBeTruthy();
            const auto a = render(whole.editor.timeline(), 0, 48000);
            const auto b = render(split.editor.timeline(), 0, 48000);
            expect(a.size() == b.size() && !a.empty()).toBeTruthy();
            // The decoder continues across the cut, so nothing is re-decoded or resampled anew.
            expect(a == b).toBeTruthy();
        });

        it("plays detached sound exactly like the video clip's own", {
            if (!have_fixture("h264_30fps_aac.mp4")) {
                return;
            }
            Sequence joined = make_sequence();
            expect(place_camera(joined, 4800).valid()).toBeTruthy();
            Sequence detached = make_sequence();
            const tl::ClipId video = place_camera(detached, 4800);
            expect(detached.editor
                       .execute(tl::edit::detach_audio(video, detached.audio,
                                                       detached.editor.new_clip_id()))
                       .has_value())
                .toBeTruthy();
            const auto a = render(joined.editor.timeline(), 0, 60000);
            const auto b = render(detached.editor.timeline(), 0, 60000);
            expect(peak(a, 4800, 52800) > 0.05F).toBeTruthy();
            expect(a == b).toBeTruthy();
        });

        it("renders the same samples from a later start as in one pass", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            Sequence seq = make_sequence();
            expect(place(seq, 0, 0, 48000).valid()).toBeTruthy();
            const auto whole = render(seq.editor.timeline(), 0, 48000);
            const auto later = render(seq.editor.timeline(), 12000, 12000);
            float worst = 0.0F;
            for (std::size_t i = 0; i < later.size(); ++i) {
                worst = std::max(worst, std::abs(later[i] - whole[(12000 * kChannels) + i]));
            }
            // A seek restarts the resampler, so the start differs by its filter transient only.
            expect(worst < 0.02F).toBeTruthy();
        });
    });
}
