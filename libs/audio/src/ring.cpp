#include "oma/audio/ring.hpp"

#include <algorithm>
#include <bit>

namespace oma::audio {

SampleRing::SampleRing(std::size_t capacity_frames, int channels)
    : channels_(std::max(channels, 1)),
      mask_(std::bit_ceil(std::max<std::size_t>(capacity_frames, 2)) - 1),
      data_((mask_ + 1) * static_cast<std::size_t>(channels_)) {}

std::size_t SampleRing::writable() const noexcept {
    const std::uint64_t head = head_.load(std::memory_order_relaxed);
    const std::uint64_t tail = tail_.load(std::memory_order_acquire);
    return capacity() - static_cast<std::size_t>(head - tail);
}

std::size_t SampleRing::readable() const noexcept {
    const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint64_t head = head_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(head - tail);
}
void SampleRing::discard_readable() noexcept {
    tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
}

void SampleRing::discard_until(std::uint64_t mark) noexcept {
    const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint64_t head = head_.load(std::memory_order_acquire);
    const std::uint64_t to = std::min(mark, head);
    if (to > tail) {
        tail_.store(to, std::memory_order_release);
    }
}

int SampleRing::channels() const noexcept { return channels_; }
std::size_t SampleRing::capacity() const noexcept { return mask_ + 1; }
std::uint64_t SampleRing::written() const noexcept { return head_.load(std::memory_order_acquire); }

} // namespace oma::audio
