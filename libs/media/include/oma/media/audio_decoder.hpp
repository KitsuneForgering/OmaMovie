#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"
#include "oma/media/probe.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace oma::media {

// Decoded audio as planar float32: channel c occupies samples[c * frames, (c + 1) * frames).
struct AudioBuffer {
    RationalTime pts; // first sample, in 1/sample_rate units
    SampleRate sample_rate;
    int channels = 0;
    std::int64_t frames = 0;
    std::vector<float> samples;

    [[nodiscard]] std::span<const float> channel(int c) const noexcept;
};

struct AudioDecoderOptions {
    std::optional<int> stream;             // stream index; default: the best audio stream
    std::optional<SampleRate> sample_rate; // output rate; default: the stream's
    std::optional<int> channels; // output channels (default layout); default: the stream's
};

// Decodes one audio stream to planar float32 at a fixed rate and layout (CLAUDE.md §12). Not
// thread-safe; never call from the real-time audio thread (it allocates and does I/O).
class AudioDecoder {
public:
    [[nodiscard]] static Result<std::unique_ptr<AudioDecoder>>
    open(const std::filesystem::path& path, const AudioDecoderOptions& options = {});

    ~AudioDecoder();
    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;
    AudioDecoder(AudioDecoder&&) = delete;
    AudioDecoder& operator=(AudioDecoder&&) = delete;

    [[nodiscard]] const StreamInfo& stream() const noexcept;
    [[nodiscard]] SampleRate sample_rate() const noexcept;
    [[nodiscard]] int channels() const noexcept;

    // The next buffer, or std::nullopt at the end of the stream.
    [[nodiscard]] Result<std::optional<AudioBuffer>> next();

    // Makes the next buffer start exactly at `t` (rounded down to a sample).
    [[nodiscard]] Result<void> seek(const RationalTime& t);

private:
    struct Impl;
    explicit AudioDecoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::media
