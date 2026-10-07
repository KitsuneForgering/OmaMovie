#include "oma/audio/output.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace oma::audio {

namespace {

constexpr std::size_t kRingSeconds = 2;

class NullOutput final : public AudioOutput {
public:
    NullOutput(const OutputFormat& format, std::chrono::milliseconds period)
        : format_(format), period_(period),
          ring_(static_cast<std::size_t>(format.rate.hz()) * kRingSeconds, format.channels) {}

    NullOutput(const NullOutput&) = delete;
    NullOutput& operator=(const NullOutput&) = delete;
    NullOutput(NullOutput&&) = delete;
    NullOutput& operator=(NullOutput&&) = delete;
    ~NullOutput() override { stop(); }

    [[nodiscard]] const OutputFormat& format() const noexcept override { return format_; }
    [[nodiscard]] SampleRing& ring() noexcept override { return ring_; }

    [[nodiscard]] Result<void> start() override {
        if (thread_.joinable()) {
            return {};
        }
        thread_ = std::jthread([this](const std::stop_token& stop) { run(stop); });
        return {};
    }

    void stop() noexcept override {
        if (thread_.joinable()) {
            thread_.request_stop();
            thread_.join();
        }
    }

    [[nodiscard]] std::int64_t frames_consumed() const noexcept override {
        return consumed_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::int64_t latency_frames() const noexcept override { return 0; }
    [[nodiscard]] std::int64_t underruns() const noexcept override {
        return underruns_.load(std::memory_order_relaxed);
    }

    void set_muted(bool /*muted*/) noexcept override {}
    [[nodiscard]] bool healthy() const noexcept override { return true; }

    void flush() noexcept override {
        if (!thread_.joinable()) {
            ring_.discard_until(ring_.written()); // no consumer running
            return;
        }
        flush_mark_.store(ring_.written(), std::memory_order_relaxed);
        flush_requested_.store(true, std::memory_order_release);
    }

private:
    // The "device": every period, consume the frames that elapsed in real time.
    void run(const std::stop_token& stop) {
        const auto rate = static_cast<std::int64_t>(format_.rate.hz());
        std::vector<float> scratch;
        const auto start = std::chrono::steady_clock::now();
        std::int64_t due_total = 0;
        while (!stop.stop_requested()) {
            if (flush_requested_.exchange(false, std::memory_order_acq_rel)) {
                ring_.discard_until(flush_mark_.load(std::memory_order_relaxed));
            }
            const auto elapsed = std::chrono::steady_clock::now() - start;
            const std::int64_t due =
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() * rate /
                1'000'000;
            const auto frames = static_cast<std::size_t>(due - due_total);
            due_total = due;
            scratch.resize(frames * static_cast<std::size_t>(format_.channels));
            const std::size_t got = ring_.read(scratch);
            if (got < frames) {
                underruns_.fetch_add(1, std::memory_order_relaxed);
            }
            consumed_.fetch_add(static_cast<std::int64_t>(got), std::memory_order_release);
            std::this_thread::sleep_for(period_);
        }
        // A flush requested while stopping must not wait forever.
        flush_requested_.store(false, std::memory_order_release);
    }

    OutputFormat format_;
    std::chrono::milliseconds period_;
    SampleRing ring_;
    std::atomic<std::int64_t> consumed_{0};
    std::atomic<std::int64_t> underruns_{0};
    std::atomic<bool> flush_requested_{false};
    std::atomic<std::uint64_t> flush_mark_{0};
    std::jthread thread_;
};

} // namespace

Result<std::unique_ptr<AudioOutput>> make_null_output(const OutputFormat& format,
                                                      std::chrono::milliseconds period) {
    if (format.channels <= 0 || period.count() <= 0) {
        return make_error(ErrorCode::InvalidArgument, Category::Audio, "invalid output format");
    }
    return std::unique_ptr<AudioOutput>(std::make_unique<NullOutput>(format, period));
}

} // namespace oma::audio
