#include "oma/media/format.hpp"

#include "ffmpeg.hpp"

namespace oma::media {

Result<MediaInfo> FfmpegFormatBackend::inspect_input(const std::filesystem::path& path) const {
    return probe(path);
}

Result<void> FfmpegFormatBackend::check_output(const ExportFormat& format) const {
    ff::ensure_initialized();
    if (format.container.empty() || format.video_encoder.empty() ||
        (format.audio_encoder && format.audio_encoder->empty())) {
        return make_error(ErrorCode::InvalidArgument, Category::Encode,
                          "missing output format or encoder");
    }
    const AVOutputFormat* muxer = av_guess_format(format.container.c_str(), nullptr, nullptr);
    if (muxer == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Encode,
                          "output container is unavailable", format.container);
    }
    const auto check_encoder = [&](const std::string& name, AVMediaType kind) -> Result<void> {
        const AVCodec* encoder = avcodec_find_encoder_by_name(name.c_str());
        if (encoder == nullptr || encoder->type != kind) {
            return make_error(ErrorCode::Unsupported, Category::Encode,
                              "output encoder is unavailable", name);
        }
        if (avformat_query_codec(muxer, encoder->id, FF_COMPLIANCE_NORMAL) != 1) {
            return make_error(ErrorCode::Unsupported, Category::Encode,
                              "encoder is not supported by the output container",
                              format.container + ":" + name);
        }
        return {};
    };
    if (auto checked = check_encoder(format.video_encoder, AVMEDIA_TYPE_VIDEO); !checked) {
        return checked;
    }
    if (format.audio_encoder) {
        return check_encoder(*format.audio_encoder, AVMEDIA_TYPE_AUDIO);
    }
    return {};
}

} // namespace oma::media
