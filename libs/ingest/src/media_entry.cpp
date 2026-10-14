#include "oma/ingest/media_entry.hpp"

namespace oma::ingest {

std::optional<MediaEntry> media_entry(const oma::media::MediaInfo& probed,
                                      oma::timeline::MediaId id) {
    if (!probed.best_video && !probed.best_audio)
        return std::nullopt;
    // Music, voiceover and sound effects are audio-only media; they go on the audio lanes.
    const bool audio_only = !probed.best_video;
    const auto& stream = probed.streams[static_cast<std::size_t>(audio_only ? *probed.best_audio
                                                                            : *probed.best_video)];
    MediaEntry entry;
    entry.audio_only = audio_only;
    entry.info.id = id;
    entry.info.start = stream.start.value_or(*oma::RationalTime::make(0, stream.timebase));
    entry.length = stream.duration ? stream.duration : probed.duration;
    entry.info.duration =
        entry.length ? entry.length->rescaled(entry.info.start.timebase(), oma::Rounding::Floor)
                           .value_or(oma::RationalTime{})
                     : *oma::RationalTime::make(0, entry.info.start.timebase());
    entry.info.has_video = !audio_only;
    entry.info.has_audio = probed.best_audio.has_value();
    entry.info.still = stream.video && stream.video->still_image;
    return entry;
}

} // namespace oma::ingest
