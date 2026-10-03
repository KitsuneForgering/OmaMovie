#include "ffmpeg.hpp"

#include "oma/base/log.hpp"

#include <array>
#include <cstdarg>
#include <mutex>
#include <string_view>

namespace oma::media::ff {

namespace {

LogLevel to_log_level(int av_level) noexcept {
    if (av_level <= AV_LOG_ERROR) {
        return LogLevel::Error;
    }
    if (av_level <= AV_LOG_WARNING) {
        return LogLevel::Warn;
    }
    if (av_level <= AV_LOG_INFO) {
        return LogLevel::Debug;
    }
    return LogLevel::Trace;
}

void log_callback(void* avcl, int level, const char* fmt, va_list vl) {
    if (level > AV_LOG_VERBOSE) {
        return;
    }
    const LogLevel ours = to_log_level(level);
    if (!log_enabled(Category::Media, ours)) {
        return;
    }
    // FFmpeg may emit one line in several calls; the prefix flag tracks that per thread.
    thread_local int print_prefix = 1;
    std::array<char, 1024> line{};
    av_log_format_line2(avcl, level, fmt, vl, line.data(), static_cast<int>(line.size()),
                        &print_prefix);
    std::string_view text(line.data());
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    if (!text.empty()) {
        log_message(ours, Category::Media, text);
    }
}

} // namespace

std::string error_string(int err) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buf{};
    av_strerror(err, buf.data(), buf.size());
    return buf.data();
}

Error av_error(int err, Category category, std::string message, const std::string& context) {
    ErrorCode code = ErrorCode::Internal;
    switch (err) {
    case AVERROR(ENOENT):
    case AVERROR(EACCES):
    case AVERROR(EPERM):
    case AVERROR(EIO):
    case AVERROR(EISDIR):
    case AVERROR_EOF:
        code = ErrorCode::IoError;
        break;
    case AVERROR_INVALIDDATA:
        code = ErrorCode::InvalidData;
        break;
    case AVERROR_DECODER_NOT_FOUND:
    case AVERROR_DEMUXER_NOT_FOUND:
    case AVERROR_STREAM_NOT_FOUND:
    case AVERROR_PATCHWELCOME:
    case AVERROR(ENOSYS):
        code = ErrorCode::Unsupported;
        break;
    case AVERROR_EXIT:
        code = ErrorCode::Cancelled;
        break;
    default:
        break;
    }
    std::string ctx = context.empty() ? error_string(err) : context + ": " + error_string(err);
    return {code, category, std::move(message), std::move(ctx)};
}

void ensure_initialized() {
    static std::once_flag once;
    std::call_once(once, [] { av_log_set_callback(log_callback); });
}

Result<FormatPtr> open_input(const std::filesystem::path& path) {
    ensure_initialized();
    AVFormatContext* raw = nullptr;
    if (const int err = avformat_open_input(&raw, path.c_str(), nullptr, nullptr); err < 0) {
        return std::unexpected(av_error(err, Category::Media, "cannot open media", path.string()));
    }
    FormatPtr fmt(raw);
    if (const int err = avformat_find_stream_info(fmt.get(), nullptr); err < 0) {
        return std::unexpected(
            av_error(err, Category::Media, "cannot read stream information", path.string()));
    }
    return fmt;
}

std::optional<Rational> to_rational(AVRational r) {
    if (r.den == 0) {
        return std::nullopt;
    }
    auto q = Rational::make(r.num, r.den);
    return q ? std::optional<Rational>(*q) : std::nullopt;
}

std::optional<Rational> to_timebase(AVRational r) {
    auto q = to_rational(r);
    return q && q->is_positive() ? q : std::nullopt;
}

std::optional<RationalTime> to_time(std::int64_t ts, Rational timebase) {
    if (ts == AV_NOPTS_VALUE) {
        return std::nullopt;
    }
    auto t = RationalTime::make(ts, timebase);
    return t ? std::optional<RationalTime>(*t) : std::nullopt;
}

Result<std::int64_t> to_ticks(const RationalTime& t, Rational timebase, Rounding rounding) {
    return rescale(t.value(), t.timebase(), timebase, rounding);
}

Result<CodecPtr> alloc_codec(const AVStream& stream, const AVCodec& codec) {
    CodecPtr ctx(avcodec_alloc_context3(&codec));
    if (!ctx) {
        return make_error(ErrorCode::Internal, Category::Decode, "cannot allocate a decoder");
    }
    if (const int err = avcodec_parameters_to_context(ctx.get(), stream.codecpar); err < 0) {
        return std::unexpected(av_error(err, Category::Decode, "invalid codec parameters"));
    }
    ctx->pkt_timebase = stream.time_base;
    return ctx;
}

} // namespace oma::media::ff
