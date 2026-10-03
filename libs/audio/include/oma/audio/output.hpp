#pragma once

#include "oma/audio/ring.hpp"
#include "oma/base/error.hpp"
#include "oma/base/time.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>

// Audio output (CLAUDE.md §12). The device pulls interleaved float frames from a SampleRing on its
// real-time thread; the count of frames it consumed is the playback master clock.

namespace oma::audio {

struct OutputFormat {
    SampleRate rate = SampleRate::make(48000).value();
    int channels = 2;
};

class AudioOutput {
public:
    AudioOutput() = default;
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;
    virtual ~AudioOutput() = default;

    [[nodiscard]] virtual const OutputFormat& format() const noexcept = 0;
    // The producer side belongs to one playback thread.
    [[nodiscard]] virtual SampleRing& ring() noexcept = 0;

    [[nodiscard]] virtual Result<void> start() = 0;
    virtual void stop() noexcept = 0;

    // Real frames taken from the ring since creation (silence for underruns is not counted, so
    // the clock stalls instead of running ahead of the audio). Thread-safe, monotonic.
    [[nodiscard]] virtual std::int64_t frames_consumed() const noexcept = 0;
    // Frames taken from the ring but not yet audible (device buffering), for A/V sync.
    [[nodiscard]] virtual std::int64_t latency_frames() const noexcept = 0;
    // Callbacks that found the ring short of data.
    [[nodiscard]] virtual std::int64_t underruns() const noexcept = 0;

    // Drops queued frames (seek). Returns once the consumer has dropped them; the producer must
    // not write meanwhile.
    virtual void flush() noexcept = 0;

    // Silences the output without changing its timing: the device keeps consuming at its rate,
    // so the clock stays real (measurements, a preview mute).
    virtual void set_muted(bool muted) noexcept = 0;
    // False once the output can no longer play (the stream failed or lost its device). Playback
    // then restarts on a new output from the audible position.
    [[nodiscard]] virtual bool healthy() const noexcept = 0;
};

// Consumes frames in real time without making sound, paced by a steady clock: tests, CI and
// machines without PipeWire.
[[nodiscard]] Result<std::unique_ptr<AudioOutput>>
make_null_output(const OutputFormat& format,
                 std::chrono::milliseconds period = std::chrono::milliseconds(10));

// A PipeWire playback stream. Unsupported when no PipeWire daemon answers.
[[nodiscard]] Result<std::unique_ptr<AudioOutput>>
make_pipewire_output(const OutputFormat& format, std::string_view node_name = "OmaMovie");

} // namespace oma::audio
