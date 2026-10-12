#include "oma/audio/ring.hpp"

#include <algorithm>

namespace oma::audio {

std::size_t SampleRing::write(std::span<const float> interleaved) noexcept {
    const auto ch = static_cast<std::size_t>(channels_);
    const std::size_t frames = std::min(interleaved.size() / ch, writable());
    const std::uint64_t head = head_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < frames; ++i) {
        const std::size_t slot = static_cast<std::size_t>(head + i) & mask_;
        std::copy_n(interleaved.begin() + static_cast<std::ptrdiff_t>(i * ch), ch,
                    data_.begin() + static_cast<std::ptrdiff_t>(slot * ch));
    }
    head_.store(head + frames, std::memory_order_release);
    return frames;
}

std::size_t SampleRing::read(std::span<float> interleaved) noexcept {
    const auto ch = static_cast<std::size_t>(channels_);
    const std::size_t frames = std::min(interleaved.size() / ch, readable());
    const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < frames; ++i) {
        const std::size_t slot = static_cast<std::size_t>(tail + i) & mask_;
        std::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(slot * ch), ch,
                    interleaved.begin() + static_cast<std::ptrdiff_t>(i * ch));
    }
    tail_.store(tail + frames, std::memory_order_release);
    return frames;
}

} // namespace oma::audio
