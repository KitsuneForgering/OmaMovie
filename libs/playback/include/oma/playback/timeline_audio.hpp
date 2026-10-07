#pragma once

#include "oma/audio/effect.hpp"
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
#include <vector>

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

    // Renders `timeline` from now on, keeping the open decoders of clips that still exist with
    // the same media and noise reduction: restarting playback after a seek or an edit seeks
    // them instead of reopening their files.
    void set_timeline(oma::timeline::Timeline timeline,
                      std::unordered_map<std::uint64_t, std::string> paths);

    [[nodiscard]] int channels() const noexcept { return channels_; }
    // Clips with an open decoder (tests see reuse through it).
    [[nodiscard]] std::size_t open_streams() const noexcept { return streams_.size(); }

private:
    struct Stream {
        std::uint64_t media = 0;             // media ID
        oma::timeline::NoiseReduction noise; // what the decoder was opened with
        oma::audio::EffectChain effects;     // the clip's, configured from its audio properties
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

    // Samples a clip plays, in sequence samples: [first, end) plus the lead before its first
    // sample and the tail after its last that transitions add.
    struct Span {
        std::int64_t first = 0;
        std::int64_t end = 0;
        std::int64_t lead = 0;
        std::int64_t tail = 0;
    };
    [[nodiscard]] Span span_of(const oma::timeline::Track& track, std::size_t index) const;
    [[nodiscard]] oma::Result<void> mix_clip(const oma::timeline::Clip& clip, const Span& span,
                                             std::span<float> out, std::int64_t first);
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
    std::vector<float> scratch_; // a clip's samples while its effects run, reused
    std::uint64_t pass_ = 0;
};

} // namespace oma::playback
