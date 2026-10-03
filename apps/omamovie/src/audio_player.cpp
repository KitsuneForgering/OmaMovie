#include "audio_player.hpp"

#include "timeline_audio.hpp"

#include "oma/base/log.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kBlockFrames = 1024;
// Audio rendered ahead of the device. Enough to ride out a slow decode; small enough that a
// seek restarts quickly (the ring is flushed anyway).
constexpr double kAheadSeconds = 0.25;

} // namespace

AudioPlayer::AudioPlayer(oma::SampleRate rate) : rate_(rate), clock_(rate) {}

AudioPlayer::~AudioPlayer() {
    stop();
    pipeline_.shutdown();
}

oma::Result<void> AudioPlayer::ensure_output() {
    if (output_) {
        return {};
    }
    const oma::audio::OutputFormat format{.rate = rate_, .channels = kChannels};
    if (!silent_) {
        if (auto pipewire = oma::audio::make_pipewire_output(format)) {
            output_ = std::move(*pipewire);
            on_device_ = true;
            return {};
        } else {
            // Degrade, never fail silently (CLAUDE.md §19): playback keeps an audio-paced clock.
            oma::log_warn(oma::Category::Audio, "no audio device, playing silently: {}",
                          pipewire.error().summary());
        }
    }
    auto null = oma::audio::make_null_output(format);
    if (!null) {
        return std::unexpected(null.error());
    }
    output_ = std::move(*null);
    on_device_ = false;
    return {};
}

oma::Result<void> AudioPlayer::start(oma::timeline::Timeline timeline,
                                     std::unordered_map<std::uint64_t, std::string> paths, std::int64_t from) {
    stop();
    if (output_ && !output_->healthy()) {
        // The device went away: open a new output (PipeWire picks the current default sink).
        oma::log_warn(oma::Category::Audio, "audio output lost its device; reopening");
        live_output_.store(nullptr, std::memory_order_release);
        output_->stop();
        retired_.push_back(std::move(output_));
    }
    if (auto r = ensure_output(); !r) {
        return r;
    }
    output_->set_muted(muted_);
    if (auto r = clock_.anchor(rate_.sample_to_time(from), output_->frames_consumed()); !r) {
        return r;
    }
    anchor_consumed_.store(output_->frames_consumed(), std::memory_order_relaxed);
    anchor_sample_.store(clock_.anchor_sample(), std::memory_order_relaxed);
    live_output_.store(output_.get(), std::memory_order_release);
    render_failed_.store(false, std::memory_order_relaxed);
    render_error_.clear();
    auto renderer = std::make_shared<TimelineAudio>(std::move(timeline), std::move(paths), rate_, kChannels);
    oma::audio::SampleRing& ring = output_->ring();
    const auto ahead = static_cast<std::size_t>(kAheadSeconds * rate_.hz());
    producer_ = pipeline_.submit("playback-audio", [this, renderer, &ring, ahead, from](oma::JobContext& ctx) {
        std::vector<float> block(kBlockFrames * static_cast<std::size_t>(kChannels));
        std::int64_t next = from;
        bool reported = false;
        while (!ctx.is_cancelled()) {
            const std::size_t queued = ring.readable();
            if (const int stall = stall_ms_.exchange(0, std::memory_order_relaxed); stall > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(stall));
                continue;
            }
            if (queued >= ahead || ring.writable() < kBlockFrames) {
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
                continue;
            }
            if (auto r = renderer->render(block, next); !r && !reported) {
                reported = true;
                render_error_ = r.error().summary();
                render_failed_.store(true, std::memory_order_release);
            }
            float peak = 0.0F;
            for (const float v : block) peak = std::max(peak, std::abs(v));
            if (peak > peak_.load(std::memory_order_relaxed)) peak_.store(peak, std::memory_order_relaxed);
            ring.write(block);
            next += static_cast<std::int64_t>(kBlockFrames);
        }
        return oma::Result<void>{};
    });
    // The device may request its first quantum as soon as it becomes active. Give the
    // producer time to decode two blocks before enabling that real-time callback.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (ring.readable() < 2 * kBlockFrames &&
           std::chrono::steady_clock::now() < deadline &&
           !render_failed_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (auto r = output_->start(); !r) {
        stop();
        return r;
    }
    running_ = true;
    return {};
}

void AudioPlayer::stop() noexcept {
    // Order matters for the single-producer/single-consumer ring: the producer is joined first,
    // then the device itself drops the queued frames (a flush on an active output runs in its
    // callback, which on PipeWire's data thread may still be running), and only then the device
    // stops pulling.
    if (producer_.valid()) {
        producer_.cancel();
        (void)producer_.wait();
        producer_ = {};
    }
    if (output_) {
        output_->flush();
        output_->stop();
    }
    running_ = false;
}

std::int64_t AudioPlayer::audible_sample() const noexcept {
    const oma::audio::AudioOutput* out = live_output_.load(std::memory_order_acquire);
    if (out == nullptr) {
        return anchor_sample_.load(std::memory_order_relaxed);
    }
    // PlaybackClock's rule (oma/audio/clock.hpp), on the atomic copies of its anchor.
    const std::int64_t played = out->frames_consumed() - anchor_consumed_.load(std::memory_order_relaxed) -
                                out->latency_frames();
    return anchor_sample_.load(std::memory_order_relaxed) + std::max<std::int64_t>(played, 0);
}

bool AudioPlayer::healthy() const noexcept {
    return !output_ || output_->healthy();
}

void AudioPlayer::set_muted(bool muted) noexcept {
    muted_ = muted;
    if (output_) output_->set_muted(muted);
}

std::int64_t AudioPlayer::underruns() const noexcept {
    return output_ ? output_->underruns() : 0;
}

std::optional<std::string> AudioPlayer::take_error() {
    if (!render_failed_.exchange(false, std::memory_order_acquire)) {
        return std::nullopt;
    }
    return render_error_;
}
