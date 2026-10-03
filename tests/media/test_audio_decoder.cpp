#include "oma/media/audio_decoder.hpp"

#include "media_test.hpp"

#include <cmath>
#include <cstdint>
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
        it("decodes AAC and Opus", { decodes_aac_and_opus(); });
        it("seeks to an exact sample", { seeks_to_the_sample(); });
        it("fails on files without audio", { rejects_video_only(); });
    });
}
