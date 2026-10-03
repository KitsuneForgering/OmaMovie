#pragma once

// FFmpeg plumbing shared by libs/media sources: RAII owners, error and time conversion.
// Private to libs/media (CLAUDE.md §5.2: libav* headers never leave this library).

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace oma::media::ff {

struct FormatCloser {
    void operator()(AVFormatContext* p) const noexcept { avformat_close_input(&p); }
};
struct CodecFreer {
    void operator()(AVCodecContext* p) const noexcept { avcodec_free_context(&p); }
};
struct FrameFreer {
    void operator()(AVFrame* p) const noexcept { av_frame_free(&p); }
};
struct PacketFreer {
    void operator()(AVPacket* p) const noexcept { av_packet_free(&p); }
};
struct BufferUnref {
    void operator()(AVBufferRef* p) const noexcept { av_buffer_unref(&p); }
};
struct SwrFreer {
    void operator()(SwrContext* p) const noexcept { swr_free(&p); }
};

using FormatPtr = std::unique_ptr<AVFormatContext, FormatCloser>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecFreer>;
using FramePtr = std::unique_ptr<AVFrame, FrameFreer>;
using PacketPtr = std::unique_ptr<AVPacket, PacketFreer>;
using BufferPtr = std::unique_ptr<AVBufferRef, BufferUnref>;
using SwrPtr = std::unique_ptr<SwrContext, SwrFreer>;

[[nodiscard]] std::string error_string(int err);

// Maps an AVERROR to OmaMovie's error codes; the FFmpeg message goes into the context.
[[nodiscard]] Error av_error(int err, Category category, std::string message,
                             const std::string& context = {});

// Routes FFmpeg's log through OmaMovie's logger (Category::Media). Idempotent.
void ensure_initialized();

// Opens a file and reads its stream information.
[[nodiscard]] Result<FormatPtr> open_input(const std::filesystem::path& path);

// AVRational -> Rational; std::nullopt when FFmpeg reports an unknown value (0/0, x/0).
[[nodiscard]] std::optional<Rational> to_rational(AVRational r);
// A positive timebase, or std::nullopt.
[[nodiscard]] std::optional<Rational> to_timebase(AVRational r);
// A timestamp in `timebase`; AV_NOPTS_VALUE becomes std::nullopt (CLAUDE.md §6).
[[nodiscard]] std::optional<RationalTime> to_time(std::int64_t ts, Rational timebase);
// A time expressed in `timebase` ticks.
[[nodiscard]] Result<std::int64_t> to_ticks(const RationalTime& t, Rational timebase,
                                            Rounding rounding);

// A decoder context configured from the stream, not yet opened: callers attach a hardware
// device first when they want one, then call avcodec_open2.
[[nodiscard]] Result<CodecPtr> alloc_codec(const AVStream& stream, const AVCodec& codec);

} // namespace oma::media::ff
