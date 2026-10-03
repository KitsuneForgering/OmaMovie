#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"
#include "oma/media/audio_decoder.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

// Renders the sequence's audio, sample-accurate, at the output rate (CLAUDE.md §6, §12): for each
// block, every audible clip's media is decoded at the right position and mixed with its gain and
// fades. Works on a snapshot of the timeline taken when playback starts.
//
// Threading: owned by the playback audio pipeline thread; never the real-time callback (it
// decodes, allocates and does I/O).

namespace oma::playback {

class TimelineAudio {
public:
    TimelineAudio(oma::timeline::Timeline timeline,
                  std::unordered_map<std::uint64_t, std::string> paths, oma::SampleRate rate,
                  int channels);

    // Fills `out` (interleaved) with sequence samples [first, first + out.size() / channels).
    // Clips at speeds other than 1 stay silent until time-stretching exists. Media that cannot
    // be decoded is silent and reported once through the result.
    [[nodiscard]] oma::Result<void> render(std::span<float> out, std::int64_t first);

    [[nodiscard]] int channels() const noexcept { return channels_; }

private:
    struct Stream {
        std::uint64_t media = 0; // media ID
        std::unique_ptr<oma::media::AudioDecoder> decoder;
        std::optional<oma::media::AudioBuffer> buffer;
        std::int64_t offset = 0;      // frames of `buffer` already used
        std::int64_t next_sample = 0; // media sample at `buffer[offset]`
        bool ended = false;
        bool failed = false;
        std::uint64_t used_in = 0; // last render pass that touched it
        // Its clip has played through its last sample: a clip continuing the same media at
        // `next_sample` (the right part of a split) takes the decoder over without a seek.
        bool finished = false;
    };

    [[nodiscard]] oma::Result<void> mix_clip(const oma::timeline::Clip& clip, std::span<float> out,
                                             std::int64_t first);
    // The clip's stream: its own, one handed over by a finished clip of the same media that
    // stopped at `media_sample`, or a newly opened decoder.
    [[nodiscard]] oma::Result<Stream*> stream(const oma::timeline::Clip& clip,
                                              std::int64_t media_sample);

    oma::timeline::Timeline timeline_;
    std::unordered_map<std::uint64_t, std::string> paths_; // media ID -> file
    oma::SampleRate rate_;
    int channels_;
    std::int64_t ticks_per_sample_ = 1;
    std::unordered_map<std::uint64_t, Stream> streams_; // per clip ID
    std::uint64_t pass_ = 0;
};

} // namespace oma::playback
