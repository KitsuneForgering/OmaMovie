#pragma once

#include "oma/audio/clock.hpp"
#include "oma/audio/output.hpp"
#include "oma/base/error.hpp"
#include "oma/base/jobs.hpp"
#include "oma/timeline/model.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace oma::playback {
class TimelineAudio;
}

// Plays the sequence's audio and provides the playback master clock (CLAUDE.md §12): a pipeline
// thread renders timeline audio into the output's lock-free ring, the device consumes it, and
// the audible position comes from the frames the device actually consumed minus its latency.
// Video follows that position, dropping or repeating frames; audio is never stretched.
//
// Threading: start/stop/position are called on the UI thread. The pipeline runs on its own
// one-thread JobPool (the "playback" pipeline of ADR-0003) and is the ring's only producer; the
// device callback is its only consumer. stop() cancels and joins the pipeline before the ring
// is flushed, so the two never write concurrently.
class AudioPlayer {
public:
    static constexpr int kChannels = 2;

    explicit AudioPlayer(oma::SampleRate rate);
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;

    // Starts playing `timeline` from sequence sample `from`. Media IDs map to files in `paths`.
    [[nodiscard]] oma::Result<void> start(oma::timeline::Timeline timeline,
                                          std::unordered_map<std::uint64_t, std::string> paths,
                                          std::int64_t from);
    void stop() noexcept;
    [[nodiscard]] bool running() const noexcept { return running_; }
    // Plays into the silent null output even when a device exists (automated runs).
    void set_silent(bool silent) noexcept { silent_ = silent; }
    // Highest absolute sample rendered since the last call (a level meter; also how tests see
    // that clip audio is actually mixed).
    [[nodiscard]] float take_peak() noexcept { return peak_.exchange(0.0F, std::memory_order_relaxed); }

    // The sequence sample audible now. It holds still until the device plays real samples.
    // Safe from any thread (the viewer's presentation measurement reads it on the render
    // thread); during a restart it may briefly report the previous anchor.
    [[nodiscard]] std::int64_t audible_sample() const noexcept;
    // False when the device stream failed or lost its device; the caller restarts playback,
    // which opens a new output.
    [[nodiscard]] bool healthy() const noexcept;
    // Device-level mute for measurements: the device clock keeps running.
    void set_muted(bool muted) noexcept;
    // Diagnostics: the pipeline stops producing for `ms`, forcing underruns.
    void inject_stall(int ms) noexcept { stall_ms_.store(ms, std::memory_order_relaxed); }
    // Whether sound reaches a device (PipeWire) rather than the silent null output.
    [[nodiscard]] bool on_device() const noexcept { return on_device_; }
    [[nodiscard]] std::int64_t underruns() const noexcept;
    // Rendering failures (undecodable media) since start, reported once per start.
    [[nodiscard]] std::optional<std::string> take_error();

private:
    [[nodiscard]] oma::Result<void> ensure_output();

    oma::SampleRate rate_;
    std::unique_ptr<oma::audio::AudioOutput> output_;
    oma::audio::PlaybackClock clock_;
    bool on_device_ = false;
    bool silent_ = false;
    std::atomic<float> peak_{0.0F};
    bool muted_ = false;
    std::atomic<int> stall_ms_{0};
    // Mirrors of the clock's anchor for lock-free reads from other threads.
    std::atomic<std::int64_t> anchor_sample_{0};
    std::atomic<std::int64_t> anchor_consumed_{0};
    std::atomic<oma::audio::AudioOutput*> live_output_{nullptr};
    // Outputs replaced after losing their device stay alive until the player goes: another
    // thread may still be reading the one it loaded. Replacement is rare (device loss).
    std::vector<std::unique_ptr<oma::audio::AudioOutput>> retired_;
    bool running_ = false;
    std::atomic<bool> render_failed_{false};
    std::string render_error_; // written by the pipeline before setting render_failed_
    // Kept across start() calls so restarts reuse its decoders; only the producer uses it
    // while playing.
    std::shared_ptr<oma::playback::TimelineAudio> renderer_;
    oma::JobHandle producer_;
    oma::JobPool pipeline_{1}; // destroyed first: the producer never outlives the output
};
