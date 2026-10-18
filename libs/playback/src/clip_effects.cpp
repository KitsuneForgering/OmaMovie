#include "clip_effects.hpp"

#include "oma/audio/equalizer.hpp"

#include <cstddef>

namespace oma::playback {

namespace {

// Chain slots, in processing order.
constexpr std::size_t kEqualizerSlot = 0;
constexpr std::size_t kSlots = 1;

} // namespace

void configure_effects(oma::audio::EffectChain& chain, const oma::timeline::AudioProperties& audio,
                       std::int32_t sample_rate) {
    chain.at<oma::audio::Equalizer>(kEqualizerSlot)
        .configure(
            sample_rate,
            {.low_db = audio.eq.low_db, .mid_db = audio.eq.mid_db, .high_db = audio.eq.high_db});
    chain.resize(kSlots);
}

} // namespace oma::playback
