#pragma once

#include "oma/audio/effect.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>

namespace oma::playback {

// Sets a clip's audio effect chain from its audio properties, in processing order. Effects
// already in their slot keep their state. A new clip audio effect needs its fields in
// timeline::AudioProperties, an audio::Effect, and a slot here. Noise reduction is not in the
// chain: it runs in the decoder (media::AudioDecoderOptions), which needs FFmpeg.
void configure_effects(oma::audio::EffectChain& chain, const oma::timeline::AudioProperties& audio,
                       std::int32_t sample_rate);

} // namespace oma::playback
