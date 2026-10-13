#include "speed_test.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {
// The tone played at `speed`, or 0 when silent (or the fixture is missing).
double tone_at(oma::Rational speed) {
    if (!have_fixture("tone_44100.wav"))
        return 0.0;
    Sequence seq = make_sequence();
    (void)place_at_speed(seq, 0, 0, 24000, speed);
    const auto out = render(seq.editor.timeline(), 0, 24000);
    return out.empty() || peak(out, 0, 24000) < 0.1F ? 0.0 : frequency(out, 0, 24000);
}
} // namespace

void run_retimed_audio_tests() {
    describe("playback::TimelineAudio at constant speed", {
        it("resamples the sound with the picture: pitch follows speed", {
            if (!have_fixture("tone_44100.wav"))
                return;
            expect(std::abs(tone_at(q(2, 1)) - 880.0) < 6.0).toBeTruthy();
            expect(std::abs(tone_at(q(1, 2)) - 220.0) < 6.0).toBeTruthy();
            expect(std::abs(tone_at(q(1, 1)) - 440.0) < 6.0).toBeTruthy();
        });

        it("plays a split double-speed clip exactly like the unsplit one", {
            if (!have_fixture("tone_44100.wav"))
                return;
            Sequence whole = make_sequence();
            Sequence split = make_sequence();
            expect(place_at_speed(whole, 0, 0, 24000, q(2, 1)).valid()).toBeTruthy();
            // Sequence sample 12000 at 2x is media sample 22050 at 44.1 kHz.
            expect(place_at_speed(split, 0, 0, 12000, q(2, 1)).valid()).toBeTruthy();
            expect(place_at_speed(split, 12000, 22050, 12000, q(2, 1)).valid()).toBeTruthy();
            const auto a = render(whole.editor.timeline(), 0, 24000);
            expect(!a.empty() && a == render(split.editor.timeline(), 0, 24000)).toBeTruthy();
        });

        it("stays silent where the decode rate would be fractional (7/11)",
           { expect(tone_at(q(7, 11))).toEqual(0.0); });
    });
}
