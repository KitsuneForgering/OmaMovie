#include "oma/playback/waveform.hpp"

#include "oma/media/audio_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace oma::playback {

namespace {

// Buckets [first, last) covering media seconds [from, to) of a waveform with `count` buckets.
std::pair<std::size_t, std::size_t> buckets(const Waveform& w, std::size_t count, double from,
                                            double to) {
    if (count == 0 || w.bucket_frames <= 0 || w.sample_rate <= 0 || !(to > from)) {
        return {0, 0};
    }
    const double per_second =
        static_cast<double>(w.sample_rate) / static_cast<double>(w.bucket_frames);
    const auto last = static_cast<double>(count);
    const double a = std::clamp(std::floor(from * per_second), 0.0, last);
    const double b = std::clamp(std::ceil(to * per_second), a, last);
    return {static_cast<std::size_t>(a), static_cast<std::size_t>(b)};
}

float rms_of(double squares, std::int64_t frames, int channels) {
    const double n = static_cast<double>(frames) * static_cast<double>(std::max(channels, 1));
    return static_cast<float>(std::sqrt(squares / n));
}

} // namespace

float Waveform::peak_between(double from, double to) const noexcept {
    const auto range = buckets(*this, peaks.size(), from, to);
    float p = 0.0F;
    for (std::size_t i = range.first; i < range.second; ++i) {
        p = std::max(p, peaks[i]);
    }
    return p;
}

std::optional<float> Waveform::noise_floor_db(double from, double to) const {
    const auto range = buckets(*this, rms.size(), from, to);
    if (range.first == range.second) {
        return std::nullopt;
    }
    std::vector<float> levels(rms.begin() + static_cast<std::ptrdiff_t>(range.first),
                              rms.begin() + static_cast<std::ptrdiff_t>(range.second));
    const auto tenth = levels.begin() + static_cast<std::ptrdiff_t>(levels.size() / 10);
    std::ranges::nth_element(levels, tenth);
    if (*tenth <= 0.0F) {
        return std::nullopt;
    }
    return static_cast<float>(20.0 * std::log10(static_cast<double>(*tenth)));
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
        w.rms.reserve(static_cast<std::size_t>(expected) + 1);
    }
    float bucket = 0.0F;
    double squares = 0.0;    // sum of squared samples in the current bucket, every channel
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
                squares += static_cast<double>(v) * static_cast<double>(v);
            }
            if (++filled == w.bucket_frames) {
                w.peaks.push_back(bucket);
                w.rms.push_back(rms_of(squares, filled, buffer.channels));
                bucket = 0.0F;
                squares = 0.0;
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
        w.rms.push_back(rms_of(squares, filled, d.channels()));
    }
    return w;
}

} // namespace oma::playback
