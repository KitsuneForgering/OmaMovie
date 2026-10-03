#include "oma/playback/waveform.hpp"

#include "oma/media/audio_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace oma::playback {

float Waveform::peak_between(double from, double to) const noexcept {
    if (peaks.empty() || bucket_frames <= 0 || sample_rate <= 0 || !(to > from)) {
        return 0.0F;
    }
    const double per_second = static_cast<double>(sample_rate) / static_cast<double>(bucket_frames);
    const auto last = static_cast<double>(peaks.size());
    const double a = std::clamp(std::floor(from * per_second), 0.0, last);
    const double b = std::clamp(std::ceil(to * per_second), a, last);
    float p = 0.0F;
    for (auto i = static_cast<std::size_t>(a); i < static_cast<std::size_t>(b); ++i) {
        p = std::max(p, peaks[i]);
    }
    return p;
}

Result<Waveform> compute_waveform(const std::filesystem::path& path, JobContext& job) {
    // The stream's own rate and layout: no resampling or downmix just to draw.
    auto decoder = media::AudioDecoder::open(path);
    if (!decoder) {
        return std::unexpected(decoder.error());
    }
    media::AudioDecoder& d = **decoder;
    Waveform w;
    w.sample_rate = d.sample_rate().hz();
    w.bucket_frames = std::max<std::int64_t>(1, w.sample_rate / kWaveformBucketsPerSecond);
    w.start = d.stream().start.value_or(RationalTime{});
    const double total = d.stream().duration ? d.stream().duration->seconds_approx() : 0.0;
    if (total > 0.0) {
        // Bounded by the declared duration; a lying header only costs reallocation.
        const double expected =
            std::min(total, 24.0 * 3600.0) * static_cast<double>(kWaveformBucketsPerSecond);
        w.peaks.reserve(static_cast<std::size_t>(expected) + 1);
    }
    float bucket = 0.0F;
    std::int64_t filled = 0; // frames in the current bucket
    std::int64_t decoded = 0;
    while (true) {
        if (job.is_cancelled()) {
            return make_error(ErrorCode::Cancelled, Category::Audio, "waveform cancelled",
                              path.string());
        }
        auto next = d.next();
        if (!next) {
            return std::unexpected(next.error());
        }
        if (!*next) {
            break;
        }
        const media::AudioBuffer& buffer = **next;
        for (std::int64_t i = 0; i < buffer.frames; ++i) {
            for (int c = 0; c < buffer.channels; ++c) {
                const float v = std::abs(buffer.channel(c)[static_cast<std::size_t>(i)]);
                bucket = std::max(bucket, std::min(v, 1.0F));
            }
            if (++filled == w.bucket_frames) {
                w.peaks.push_back(bucket);
                bucket = 0.0F;
                filled = 0;
            }
        }
        decoded += buffer.frames;
        if (total > 0.0) {
            job.report_progress(static_cast<double>(decoded) / static_cast<double>(w.sample_rate) /
                                total);
        }
    }
    if (filled > 0) {
        w.peaks.push_back(bucket);
    }
    return w;
}

} // namespace oma::playback
