#pragma once

#include "oma/base/error.hpp"
#include "oma/base/jobs.hpp"
#include "oma/base/time.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

// Waveform peaks of a media file's audio for the timeline (ui-design §7.2). Computed off the UI
// thread through the job system; plain data afterwards, shared read-only between threads.

namespace oma::playback {

struct Waveform {
    // Media time of the first sample: bucket i covers the samples from
    // start + i * bucket_frames / sample_rate on.
    RationalTime start;
    std::int32_t sample_rate = 0; // Hz, the stream's own
    std::int64_t bucket_frames = 0;
    // The largest absolute sample of each bucket over every channel, in [0, 1] (clipped).
    std::vector<float> peaks;
    // The root mean square of each bucket over every channel.
    std::vector<float> rms;

    // The largest peak over the buckets covering media seconds [from, to) relative to `start`;
    // for drawing only (CLAUDE.md §6: seconds as doubles for display).
    [[nodiscard]] float peak_between(double from, double to) const noexcept;

    // The noise level of media seconds [from, to): the RMS (dBFS) the quietest tenth of its
    // buckets stays under, the usual estimate when the material has pauses. std::nullopt for an
    // empty range or digital silence.
    [[nodiscard]] std::optional<float> noise_floor_db(double from, double to) const;
};

// Buckets per second of media: fine enough for single-frame zoom at 60 fps, small enough to
// keep an hour of sound under 1.5 MB.
inline constexpr std::int64_t kWaveformBucketsPerSecond = 100;

// A waveform as bytes for the disk cache (ADR-0009), and back. The bytes carry a version and
// their own sizes; anything that does not parse exactly (another version, truncation, counts
// that disagree or exceed kMaxWaveformBuckets) is an error, never a partial waveform.
inline constexpr std::size_t kMaxWaveformBuckets =
    std::size_t{100} * 60 * 60 * 24; // a day at 100/s
[[nodiscard]] std::vector<std::uint8_t> to_bytes(const Waveform& waveform);
[[nodiscard]] Result<Waveform> waveform_from_bytes(std::span<const std::uint8_t> bytes);

// Decodes the best audio stream at its own rate and keeps one peak per bucket. Reports progress
// and stops with ErrorCode::Cancelled when the job is cancelled.
[[nodiscard]] Result<Waveform> compute_waveform(const std::filesystem::path& path, JobContext& job);

} // namespace oma::playback
