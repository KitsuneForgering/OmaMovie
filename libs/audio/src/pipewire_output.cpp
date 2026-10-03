#include "oma/audio/output.hpp"

#include "oma/base/log.hpp"

// PipeWire and SPA are C headers full of macros; they are included as system headers (see
// module.mk) and only used in this file.
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace oma::audio {

namespace {

constexpr std::size_t kRingSeconds = 2;
constexpr int kConnectTimeoutSeconds = 2;

class PipeWireOutput final : public AudioOutput {
public:
    explicit PipeWireOutput(const OutputFormat& format)
        : format_(format),
          ring_(static_cast<std::size_t>(format.rate.hz()) * kRingSeconds, format.channels) {}

    PipeWireOutput(const PipeWireOutput&) = delete;
    PipeWireOutput& operator=(const PipeWireOutput&) = delete;
    PipeWireOutput(PipeWireOutput&&) = delete;
    PipeWireOutput& operator=(PipeWireOutput&&) = delete;

    ~PipeWireOutput() override {
        if (loop_ != nullptr) {
            pw_thread_loop_stop(loop_);
        }
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
        }
        if (loop_ != nullptr) {
            pw_thread_loop_destroy(loop_);
        }
    }

    [[nodiscard]] Result<void> connect(std::string_view node_name);

    [[nodiscard]] const OutputFormat& format() const noexcept override { return format_; }
    [[nodiscard]] SampleRing& ring() noexcept override { return ring_; }

    [[nodiscard]] Result<void> start() override {
        pw_thread_loop_lock(loop_);
        const int r = pw_stream_set_active(stream_, true);
        pw_thread_loop_unlock(loop_);
        if (r < 0) {
            return make_error(ErrorCode::Internal, Category::Audio,
                              "cannot start the PipeWire stream",
                              std::error_code(-r, std::generic_category()).message());
        }
        active_.store(true, std::memory_order_release);
        return {};
    }

    void stop() noexcept override {
        pw_thread_loop_lock(loop_);
        pw_stream_set_active(stream_, false);
        pw_thread_loop_unlock(loop_);
        active_.store(false, std::memory_order_release);
    }

    [[nodiscard]] std::int64_t frames_consumed() const noexcept override {
        return consumed_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::int64_t latency_frames() const noexcept override {
        return latency_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::int64_t underruns() const noexcept override {
        return underruns_.load(std::memory_order_relaxed);
    }

    void set_muted(bool muted) noexcept override {
        float value = muted ? 1.0F : 0.0F; // the C API takes a mutable array
        pw_thread_loop_lock(loop_);
        pw_stream_set_control(stream_, SPA_PROP_mute, 1, &value, 0);
        pw_thread_loop_unlock(loop_);
    }

    [[nodiscard]] bool healthy() const noexcept override {
        const pw_stream_state s = state_.load(std::memory_order_acquire);
        return s != PW_STREAM_STATE_ERROR && s != PW_STREAM_STATE_UNCONNECTED;
    }

    void flush() noexcept override {
        if (!active_.load(std::memory_order_acquire)) {
            ring_.discard_readable();
            return;
        }
        flush_requested_.store(true, std::memory_order_release);
        // The real-time thread handles it within one quantum; give up after a second in case
        // the graph stopped pulling (suspended sink).
        for (int i = 0; i < 1000 && flush_requested_.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (flush_requested_.exchange(false, std::memory_order_acq_rel)) {
            ring_.discard_readable();
        }
    }

private:
    static void on_process(void* data) { static_cast<PipeWireOutput*>(data)->process(); }

    static void on_state_changed(void* data, pw_stream_state /*old*/, pw_stream_state state,
                                 const char* error) {
        auto* self = static_cast<PipeWireOutput*>(data);
        self->state_.store(state, std::memory_order_release);
        if (state == PW_STREAM_STATE_ERROR && error != nullptr) {
            log_warn(Category::Audio, "PipeWire stream error: {}", error);
        }
        pw_thread_loop_signal(self->loop_, false);
    }

    // Real-time thread: no allocation, no locks, no logging.
    void process() noexcept {
        pw_buffer* b = pw_stream_dequeue_buffer(stream_);
        if (b == nullptr) {
            return;
        }
        const spa_data& d = b->buffer->datas[0];
        if (d.data == nullptr) {
            pw_stream_queue_buffer(stream_, b);
            return;
        }
        if (flush_requested_.load(std::memory_order_acquire)) {
            ring_.discard_readable();
            flush_requested_.store(false, std::memory_order_release);
        }
        const auto stride = static_cast<std::uint32_t>(sizeof(float)) *
                            static_cast<std::uint32_t>(format_.channels);
        std::uint64_t frames = d.maxsize / stride;
        if (b->requested != 0) {
            frames = std::min<std::uint64_t>(frames, b->requested);
        }
        const std::span<float> out(static_cast<float*>(d.data),
                                   frames * static_cast<std::uint64_t>(format_.channels));
        const std::size_t got = ring_.read(out);
        if (got < frames) {
            std::fill(out.begin() + static_cast<std::ptrdiff_t>(
                                        got * static_cast<std::size_t>(format_.channels)),
                      out.end(), 0.0F);
            underruns_.fetch_add(1, std::memory_order_relaxed);
        }
        consumed_.fetch_add(static_cast<std::int64_t>(got), std::memory_order_release);

        pw_time time{};
        if (pw_stream_get_time_n(stream_, &time, sizeof time) == 0 && time.rate.denom != 0) {
            // delay is in 1/rate.denom ticks; convert to output frames.
            const auto delay = static_cast<std::int64_t>(time.delay) * format_.rate.hz() *
                               static_cast<std::int64_t>(time.rate.num) /
                               static_cast<std::int64_t>(time.rate.denom);
            latency_.store(delay + static_cast<std::int64_t>(time.queued),
                           std::memory_order_relaxed);
        }

        d.chunk->offset = 0;
        d.chunk->stride = static_cast<std::int32_t>(stride);
        d.chunk->size = static_cast<std::uint32_t>(frames) * stride;
        b->size = frames;
        pw_stream_queue_buffer(stream_, b);
    }

    OutputFormat format_;
    SampleRing ring_;
    pw_thread_loop* loop_ = nullptr;
    pw_stream* stream_ = nullptr;
    pw_stream_events events_{};
    std::atomic<pw_stream_state> state_{PW_STREAM_STATE_UNCONNECTED};
    std::atomic<bool> active_{false};
    std::atomic<bool> flush_requested_{false};
    std::atomic<std::int64_t> consumed_{0};
    std::atomic<std::int64_t> latency_{0};
    std::atomic<std::int64_t> underruns_{0};
};

Result<void> PipeWireOutput::connect(std::string_view node_name) {
    static std::once_flag init;
    std::call_once(init, [] { pw_init(nullptr, nullptr); });

    loop_ = pw_thread_loop_new("oma-audio", nullptr);
    if (loop_ == nullptr) {
        return make_error(ErrorCode::Internal, Category::Audio, "cannot create the PipeWire loop");
    }
    const std::string name(node_name);
    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE,
        "Production", PW_KEY_APP_NAME, "OmaMovie", PW_KEY_NODE_NAME, name.c_str(), nullptr);
    events_.version = PW_VERSION_STREAM_EVENTS;
    events_.state_changed = on_state_changed;
    events_.process = on_process;
    stream_ =
        pw_stream_new_simple(pw_thread_loop_get_loop(loop_), name.c_str(), props, &events_, this);
    if (stream_ == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Audio,
                          "cannot create a PipeWire stream");
    }

    std::array<std::uint8_t, 1024> pod_buffer{};
    spa_pod_builder builder{};
    spa_pod_builder_init(&builder, pod_buffer.data(),
                         static_cast<std::uint32_t>(pod_buffer.size()));
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = static_cast<std::uint32_t>(format_.rate.hz());
    info.channels = static_cast<std::uint32_t>(format_.channels);
    if (format_.channels == 2) {
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
    } else if (format_.channels == 1) {
        info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    } else {
        info.flags = SPA_AUDIO_FLAG_UNPOSITIONED;
    }
    std::array<const spa_pod*, 1> params{
        spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};

    pw_thread_loop_lock(loop_);
    if (pw_thread_loop_start(loop_) < 0) {
        pw_thread_loop_unlock(loop_);
        return make_error(ErrorCode::Internal, Category::Audio, "cannot start the PipeWire loop");
    }
    const auto flags =
        static_cast<pw_stream_flags>(static_cast<unsigned>(PW_STREAM_FLAG_AUTOCONNECT) |
                                     static_cast<unsigned>(PW_STREAM_FLAG_MAP_BUFFERS) |
                                     static_cast<unsigned>(PW_STREAM_FLAG_RT_PROCESS) |
                                     static_cast<unsigned>(PW_STREAM_FLAG_INACTIVE));
    const int r =
        pw_stream_connect(stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params.data(), 1);
    // Wait until the stream is ready (paused, inactive) or failed; state changes signal the loop.
    for (int waited = 0; r >= 0 && waited < kConnectTimeoutSeconds; ++waited) {
        const pw_stream_state s = state_.load(std::memory_order_acquire);
        if (s == PW_STREAM_STATE_PAUSED || s == PW_STREAM_STATE_STREAMING ||
            s == PW_STREAM_STATE_ERROR) {
            break;
        }
        pw_thread_loop_timed_wait(loop_, 1); // seconds; releases the lock while waiting
    }
    pw_thread_loop_unlock(loop_);
    const pw_stream_state s = state_.load(std::memory_order_acquire);
    if (r < 0 || (s != PW_STREAM_STATE_PAUSED && s != PW_STREAM_STATE_STREAMING)) {
        return make_error(ErrorCode::Unsupported, Category::Audio, "no PipeWire output available",
                          r < 0 ? std::error_code(-r, std::generic_category()).message()
                                : pw_stream_state_as_string(s));
    }
    log_info(Category::Audio, "PipeWire output: {} Hz, {} channels", format_.rate.hz(),
             format_.channels);
    return {};
}

} // namespace

Result<std::unique_ptr<AudioOutput>> make_pipewire_output(const OutputFormat& format,
                                                          std::string_view node_name) {
    if (format.channels <= 0 || std::cmp_greater(format.channels, SPA_AUDIO_MAX_CHANNELS)) {
        return make_error(ErrorCode::InvalidArgument, Category::Audio, "invalid output format");
    }
    auto out = std::make_unique<PipeWireOutput>(format);
    if (auto r = out->connect(node_name); !r) {
        return std::unexpected(r.error());
    }
    return std::unique_ptr<AudioOutput>(std::move(out));
}

} // namespace oma::audio
