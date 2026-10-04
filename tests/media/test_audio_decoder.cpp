#include "oma/media/audio_decoder.hpp"

#include "media_test.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::RationalTime;
using oma::SampleRate;
using oma::media::AudioBuffer;
using oma::media::AudioDecoder;
using oma::media::AudioDecoderOptions;

namespace {

struct Totals {
    std::int64_t frames = 0;
    std::int64_t first_pts = -1;
    bool contiguous = true;
    float peak = 0.0F;
};

Totals drain(AudioDecoder& d) {
    Totals t;
    std::int64_t expected_next = -1;
    for (;;) {
        auto b = d.next();
        expect(b.has_value()).toBeTruthy();
        if (!b || !*b) {
            break;
        }
        const AudioBuffer& buf = **b;
        if (t.first_pts < 0) {
            t.first_pts = buf.pts.value();
        }
        t.contiguous = t.contiguous && (expected_next < 0 || buf.pts.value() == expected_next);
        expected_next = buf.pts.value() + buf.frames;
        t.frames += buf.frames;
        for (const float s : buf.channel(0)) {
            t.peak = std::max(t.peak, std::abs(s));
        }
    }
    return t;
}

void decodes_wav() {
    auto d = AudioDecoder::open(fixture("tone_44100.wav"));
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    expect((*d)->sample_rate().hz()).toEqual(44100);
    expect((*d)->channels()).toEqual(1);
    const Totals t = drain(**d);
    expect(t.frames).toEqual(88200);
    expect(t.first_pts).toEqual(0);
    expect(t.contiguous).toBeTruthy();
    // lavfi's sine source has amplitude 1/8.
    expect(static_cast<double>(t.peak)).toBeCloseTo(0.125, 0.01);
}

void resamples_and_upmixes() {
    AudioDecoderOptions o;
    o.sample_rate = SampleRate::make(48000).value();
    o.channels = 2;
    auto d = AudioDecoder::open(fixture("tone_44100.wav"), o);
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    expect((*d)->sample_rate().hz()).toEqual(48000);
    expect((*d)->channels()).toEqual(2);
    auto first = (*d)->next();
    expect(first && *first && (*first)->channel(1).size() == (*first)->channel(0).size())
        .toBeTruthy();
    const Totals rest = drain(**d);
    const std::int64_t total = rest.frames + (first && *first ? (*first)->frames : 0);
    // 2 s at 48 kHz, give or take the resampler's filter edges.
    expect(total).toEqual(96000);
}

// Mono sound placed on a stereo timeline plays on both channels at its recorded level (dual
// mono), not 3 dB lower as a center channel would.
void upmixes_mono_at_full_level() {
    AudioDecoderOptions o;
    o.channels = 2;
    auto d = AudioDecoder::open(fixture("tone_44100.wav"), o);
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    float left = 0.0F;
    float right = 0.0F;
    for (;;) {
        auto b = (*d)->next();
        if (!b || !*b) {
            break;
        }
        for (const float v : (*b)->channel(0)) {
            left = std::max(left, std::abs(v));
        }
        for (const float v : (*b)->channel(1)) {
            right = std::max(right, std::abs(v));
        }
    }
    expect(std::abs(left - 0.125F) < 0.002F && std::abs(right - 0.125F) < 0.002F).toBeTruthy();
}

void decodes_aac_and_opus() {
    auto aac = AudioDecoder::open(fixture("h264_30fps_aac.mp4"));
    expect(aac.has_value()).toBeTruthy();
    if (aac) {
        expect((*aac)->stream().codec).toEqual("aac");
        const Totals t = drain(**aac);
        expect(std::llabs(t.frames - 48000) <= 2048).toBeTruthy();
        expect(t.contiguous).toBeTruthy();
    }
    if (!have_fixture("av1_opus.mkv")) {
        return;
    }
    auto opus = AudioDecoder::open(fixture("av1_opus.mkv"));
    expect(opus.has_value()).toBeTruthy();
    if (opus) {
        expect((*opus)->stream().codec).toEqual("opus");
        expect((*opus)->sample_rate().hz()).toEqual(48000);
        const Totals t = drain(**opus);
        expect(std::llabs(t.frames - 48000) <= 2048).toBeTruthy();
    }
}

void seeks_to_the_sample() {
    auto d = AudioDecoder::open(fixture("tone_44100.wav"));
    if (!d) {
        return;
    }
    const auto t = (*d)->sample_rate().sample_to_time(30000);
    expect((*d)->seek(t).has_value()).toBeTruthy();
    auto b = (*d)->next();
    expect(b && *b).toBeTruthy();
    if (b && *b) {
        expect((*b)->pts.value()).toEqual(30000);
        // The tone is a pure 440 Hz sine: sample 30000 has a known value.
        const double expected =
            0.125 * std::sin(2.0 * std::numbers::pi * 440.0 * 30000.0 / 44100.0);
        expect(static_cast<double>((*b)->channel(0)[0])).toBeCloseTo(expected, 0.001);
    }
    const Totals rest = drain(**d);
    const std::int64_t first_frames = b && *b ? (*b)->frames : 0;
    expect(first_frames + rest.frames).toEqual(88200 - 30000);
}

// RMS in dBFS of channel 0 over samples [from, to) of a whole decode.
double level_db(AudioDecoder& d, std::int64_t from, std::int64_t to) {
    double squares = 0.0;
    std::int64_t n = 0;
    for (;;) {
        auto b = d.next();
        if (!b || !*b) {
            break;
        }
        const std::int64_t start = (*b)->pts.value();
        const auto plane = (*b)->channel(0);
        for (std::int64_t i = 0; i < (*b)->frames; ++i) {
            const std::int64_t at = start + i;
            if (at >= from && at < to) {
                const double v = plane[static_cast<std::size_t>(i)];
                squares += v * v;
                ++n;
            }
        }
    }
    return n > 0 ? 10.0 * std::log10(squares / static_cast<double>(n)) : -200.0;
}

AudioDecoderOptions denoised(float amount) {
    AudioDecoderOptions o;
    o.denoise = amount;
    o.noise_floor_db = -38.7F; // the fixture's hiss, measured with ffmpeg volumedetect
    return o;
}

void denoises_steady_hiss() {
    if (!have_fixture("noisy_tone.wav")) {
        return;
    }
    // The first second is hiss only; from 2 s the tone dominates.
    auto plain = AudioDecoder::open(fixture("noisy_tone.wav"));
    auto clean = AudioDecoder::open(fixture("noisy_tone.wav"), denoised(1.0F));
    expect(plain.has_value() && clean.has_value()).toBeTruthy();
    if (!plain || !clean) {
        return;
    }
    const double hiss = level_db(**plain, 24000, 48000);
    const double reduced = level_db(**clean, 24000, 48000);
    std::printf("    hiss %.1f dB -> %.1f dB\n", hiss, reduced);
    expect(hiss - reduced > 15.0).toBeTruthy();
    auto plain2 = AudioDecoder::open(fixture("noisy_tone.wav"));
    auto clean2 = AudioDecoder::open(fixture("noisy_tone.wav"), denoised(1.0F));
    const double tone = level_db(**plain2, 96000, 144000);
    const double kept = level_db(**clean2, 96000, 144000);
    expect(std::abs(tone - kept) < 1.0).toBeTruthy();
}

void denoised_seek_lands_on_the_sample() {
    if (!have_fixture("noisy_tone.wav")) {
        return;
    }
    auto d = AudioDecoder::open(fixture("noisy_tone.wav"), denoised(0.5F));
    expect(d.has_value()).toBeTruthy();
    if (!d) {
        return;
    }
    expect((*d)->seek(*RationalTime::make(100000, oma::Rational::literal(1, 48000))).has_value())
        .toBeTruthy();
    const Totals t = drain(**d);
    expect(t.first_pts).toEqual(100000);
    expect(t.contiguous).toBeTruthy();
    expect(t.frames).toEqual(144000 - 100000);
}

void rejects_bad_denoise_amounts() {
    AudioDecoderOptions o;
    o.denoise = 1.5F;
    expect(AudioDecoder::open(fixture("noisy_tone.wav"), o).has_value()).toBeFalsy();
}

void rejects_video_only() {
    auto d = AudioDecoder::open(fixture("h264_29.97fps.mp4"));
    expect(d.has_value()).toBeFalsy();
    if (!d) {
        expect(code_of(d.error())).toEqual(static_cast<int>(ErrorCode::InvalidArgument));
    }
}

} // namespace

void run_audio_decoder_tests() {
    describe("media::AudioDecoder", {
        it("decodes PCM to planar float with contiguous timestamps", { decodes_wav(); });
        it("resamples and changes the channel count", { resamples_and_upmixes(); });
        it("upmixes mono to both channels at full level", { upmixes_mono_at_full_level(); });
        it("decodes AAC and Opus", { decodes_aac_and_opus(); });
        it("seeks to an exact sample", { seeks_to_the_sample(); });
        it("fails on files without audio", { rejects_video_only(); });
        it("reduces steady hiss and keeps the tone", { denoises_steady_hiss(); });
        it("lands on the exact sample after a denoised seek",
           { denoised_seek_lands_on_the_sample(); });
        it("rejects a denoise amount outside [0, 1]", { rejects_bad_denoise_amounts(); });
    });
}
