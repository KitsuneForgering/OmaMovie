#include "oma/audio/output.hpp"

namespace oma::audio {

std::int64_t AudioOutput::frames_played(std::int64_t now_ns) const noexcept {
    (void)now_ns;
    return frames_consumed() - latency_frames();
}

} // namespace oma::audio
