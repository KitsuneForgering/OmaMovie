#include "oma/media/audio_decoder.hpp"

#include "media_test.hpp"

#include <algorithm>
#include <cmath>

#include "oma_test.hpp"

// Code review 2026-10-14: the decoder anchored on the first frame's timestamp and then counted
// samples, so audio after a timestamp gap played early for the rest of the clip.

namespace {

struct Decoded {
    std::int64_t end = -1;    // one past the last sample, in 48 kHz samples
    float gap_peak = -1.0F;   // loudest sample inside the gap [1.0, 1.5) s
    float after_peak = -1.0F; // loudest sample in [1.6, 1.9) s
};

Decoded decode_gap() {
    Decoded out;
    auto d = oma::media::AudioDecoder::open(fixture("tone_gap.mkv"));
    if (!d)
        return out;
    out.gap_peak = out.after_peak = 0.0F;
    while (auto b = (*d)->next()) {
        if (!*b)
            break;
        const oma::media::AudioBuffer& buf = **b;
        const std::int64_t first = buf.pts.value(); // pts is in samples
        for (std::int64_t i = 0; i < buf.frames; ++i) {
            const float v = std::abs(buf.channel(0)[static_cast<std::size_t>(i)]);
            const std::int64_t at = first + i;
            if (at >= 48000 + 480 && at < 72000 - 480)
                out.gap_peak = std::max(out.gap_peak, v);
            if (at >= 76800 && at < 91200)
                out.after_peak = std::max(out.after_peak, v);
        }
        out.end = first + buf.frames;
    }
    return out;
}

void check_gap() {
    if (!have_fixture("tone_gap.mkv"))
        return;
    const Decoded d = decode_gap();
    expect(std::llabs(d.end - 96000) <= 480).toBeTruthy(); // 2 s, within one packet
    expect(d.gap_peak < 0.01F).toBeTruthy();
    expect(d.after_peak > 0.1F).toBeTruthy();
}

} // namespace

void run_audio_gap_tests() {
    describe("AudioDecoder timestamp gaps", {
        it("keeps sound after a gap on its timestamps, with silence in the gap", { check_gap(); });
    });
}
