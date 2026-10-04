#include "playback_test.hpp"

#include "oma/playback/waveform.hpp"

#include <atomic>
#include <cstdio>

#include "oma_test.hpp"

using namespace playback_test;

namespace {

oma::Result<oma::playback::Waveform> waveform_of(const char* name, bool cancelled = false) {
    oma::CancellationSource source;
    if (cancelled) {
        source.cancel();
    }
    std::atomic<double> progress{0.0};
    oma::JobContext job(source.token(), progress);
    return oma::playback::compute_waveform(fixture(name), job);
}

} // namespace

void run_waveform_tests() {
    describe("playback::compute_waveform", {
        it("keeps one peak per hundredth of a second", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            auto w = waveform_of("tone_44100.wav");
            expect(w.has_value()).toBeTruthy();
            if (!w) {
                return;
            }
            expect(w->sample_rate).toBe(44100);
            expect(w->bucket_frames).toBe(441LL);
            expect(static_cast<long long>(w->peaks.size())).toBe(200LL);
            // A 440 Hz sine covers several periods per bucket: every bucket sees its peak.
            const auto range = std::ranges::minmax(w->peaks);
            expect(range.min > 0.12F && range.max < 0.13F).toBeTruthy();
        });

        it("answers peaks over a time range for drawing", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            auto w = waveform_of("tone_44100.wav");
            expect(w.has_value()).toBeTruthy();
            if (!w) {
                return;
            }
            expect(w->peak_between(0.5, 0.6) > 0.12F).toBeTruthy();
            expect(w->peak_between(2.5, 3.0)).toEqual(0.0F); // past the end
            expect(w->peak_between(0.6, 0.5)).toEqual(0.0F); // empty range
        });

        it("estimates the noise floor from the quietest buckets", {
            if (!have_fixture("noisy_tone.wav")) {
                return;
            }
            auto w = waveform_of("noisy_tone.wav");
            expect(w.has_value()).toBeTruthy();
            if (!w) {
                return;
            }
            // The hiss measures -38.7 dBFS RMS (ffmpeg volumedetect over the first second).
            const auto floor = w->noise_floor_db(0.0, 3.0);
            expect(floor.has_value()).toBeTruthy();
            std::printf("    noise floor %.1f dB\n", static_cast<double>(floor.value_or(0.0F)));
            expect(floor && std::abs(*floor + 38.7F) < 2.0F).toBeTruthy();
            expect(w->noise_floor_db(1.0, 1.0).has_value()).toBeFalsy();
        });

        it("stops when the job is cancelled", {
            if (!have_fixture("tone_44100.wav")) {
                return;
            }
            auto w = waveform_of("tone_44100.wav", true);
            expect(w.has_value()).toBeFalsy();
            expect(!w && w.error().code() == oma::ErrorCode::Cancelled).toBeTruthy();
        });

        it("fails on files without audio", {
            if (!have_fixture("still.png")) {
                return;
            }
            expect(waveform_of("still.png").has_value()).toBeFalsy();
        });
    });
}
