#include "playback_test.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

// The fixture's mono tone peaks at 1/8 on both channels; resampling to 48 kHz may overshoot
// slightly.
bool near_tone(float p, float gain) {
    return std::abs(p - (0.125F * gain)) < 0.005F;
}

tl::AudioProperties with_gain(float gain) {
    tl::AudioProperties a;
    a.gain = gain;
    return a;
}

tl::AudioProperties mid_cut() {
    tl::AudioProperties a;
    a.eq.mid_db = -12.0F;
    return a;
}

tl::AudioProperties denoised() {
    tl::AudioProperties a;
    a.noise.amount = 1.0F;
    a.noise.floor_db = -38.7F;
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

        it("keeps decoders across timeline changes that leave their clip alone", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            Sequence seq = make_sequence();
            const tl::ClipId clip = place(seq, 0, 0, 48000);
            oma::playback::TimelineAudio audio(seq.editor.timeline(), paths(), rate(), kChannels);
            std::vector<float> block(static_cast<std::size_t>(1024 * kChannels));
            expect(audio.render(block, 0).has_value()).toBeTruthy();
            expect(audio.open_streams()).toEqual(std::size_t{1});
            // A gain change keeps the decoder and takes effect, from a new position (a seek).
            expect(seq.editor.execute(tl::edit::set_audio(clip, with_gain(0.5F))).has_value())
                .toBeTruthy();
            audio.set_timeline(seq.editor.timeline(), paths());
            expect(audio.open_streams()).toEqual(std::size_t{1});
            expect(audio.render(block, 24000).has_value()).toBeTruthy();
            expect(near_tone(peak(block, 0, 1024), 0.5F)).toBeTruthy();
            // Noise reduction is a decoder setting: a new decoder.
            expect(seq.editor.execute(tl::edit::set_audio(clip, denoised())).has_value())
                .toBeTruthy();
            audio.set_timeline(seq.editor.timeline(), paths());
            expect(audio.open_streams()).toEqual(std::size_t{0});
            expect(audio.render(block, 0).has_value()).toBeTruthy();
            expect(seq.editor.execute(tl::edit::remove_clip(clip)).has_value()).toBeTruthy();
            audio.set_timeline(seq.editor.timeline(), paths());
            expect(audio.open_streams()).toEqual(std::size_t{0});
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

        it("equalizes a clip's sound", {
            if (!have_fixture("noisy_tone.wav")) {
                return;
            }
            Sequence flat = make_sequence();
            expect(place_noisy(flat).valid()).toBeTruthy();
            Sequence cut = make_sequence();
            expect(place_noisy(cut, mid_cut()).valid()).toBeTruthy();
            const auto a = render(flat.editor.timeline(), 96000, 48000);
            const auto b = render(cut.editor.timeline(), 96000, 48000);
            // The 440 Hz tone sits on the mid band's skirt: clearly quieter, not silenced.
            const double drop = level_db(a, 0, 48000) - level_db(b, 0, 48000);
            expect(drop > 3.0 && drop < 12.0).toBeTruthy();
        });

        it("reduces steady noise when asked", {
            if (!have_fixture("noisy_tone.wav")) {
                return;
            }
            Sequence plain = make_sequence();
            expect(place_noisy(plain).valid()).toBeTruthy();
            Sequence clean = make_sequence();
            expect(place_noisy(clean, denoised()).valid()).toBeTruthy();
            const auto a = render(plain.editor.timeline(), 0, 144000);
            const auto b = render(clean.editor.timeline(), 0, 144000);
            expect(level_db(a, 24000, 48000) - level_db(b, 24000, 48000) > 15.0).toBeTruthy();
            expect(std::abs(level_db(a, 96000, 144000) - level_db(b, 96000, 144000)) < 1.0)
                .toBeTruthy();
        });

        it("crossfades sound across a transition, both clips playing past the cut", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            // A plays the tone's first half second, B from 1 s; a 0.2 s dissolve into B spans
            // samples 19200 to 28800. Muting one side at a time shows what the other adds.
            const auto build = [](bool mute_a, bool mute_b) {
                Sequence seq = make_sequence();
                (void)place(seq, 0, 0, 24000, mute_a ? muted() : tl::AudioProperties{});
                const tl::ClipId b =
                    place(seq, 24000, 44100, 24000, mute_b ? muted() : tl::AudioProperties{});
                (void)seq.editor.execute(tl::edit::set_transition(
                    b, tl::Transition{.kind = tl::TransitionKind::Dissolve, .duration = s(9600)}));
                return render(seq.editor.timeline(), 0, 48000);
            };
            const auto outgoing = build(false, true);
            expect(peak(outgoing, 24000, 28000) > 0.01F).toBeTruthy(); // A past its end
            expect(peak(outgoing, 28800, 48000)).toEqual(0.0F);
            expect(near_tone(peak(outgoing, 0, 19000), 1.0F)).toBeTruthy();
            const auto incoming = build(true, false);
            expect(peak(incoming, 0, 19200)).toEqual(0.0F);
            expect(peak(incoming, 20000, 24000) > 0.01F).toBeTruthy(); // B before its start
            expect(near_tone(peak(incoming, 29000, 48000), 1.0F)).toBeTruthy();
        });

        it("plays a transition's tail when rendering starts past the cut, clips earlier", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            // A muted clip first, then A and B with a 0.2 s dissolve at 48000: starting a block
            // after the cut must still find A, two clips before the one under the playhead.
            Sequence seq = make_sequence();
            (void)place(seq, 0, 0, 24000, muted());
            (void)place(seq, 24000, 0, 24000);
            const tl::ClipId b = place(seq, 48000, 44100, 24000, muted());
            (void)seq.editor.execute(tl::edit::set_transition(
                b, tl::Transition{.kind = tl::TransitionKind::Dissolve, .duration = s(9600)}));
            const auto out = render(seq.editor.timeline(), 48000, 4000);
            expect(peak(out, 0, 4000) > 0.01F).toBeTruthy();
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
