#include "oma/media/format.hpp"

#include "ffmpeg.hpp"
#include "oma/media/audio_decoder.hpp"
#include "oma/media/video_decoder.hpp"

namespace oma::media {

ExportFormat export_format(OutputProfile profile) {
    switch (profile) {
    case OutputProfile::Mp4H264Aac:
        return {"mp4", "libx264", "aac"};
    case OutputProfile::MatroskaH264Aac:
        return {"matroska", "libx264", "aac"};
    case OutputProfile::MatroskaHevcAac:
        return {"matroska", "libx265", "aac"};
    case OutputProfile::WebmVp9Vorbis:
        return {"webm", "libvpx-vp9", "libvorbis"};
    }
    return {};
}

Result<MediaInfo> FfmpegFormatBackend::inspect_input(const std::filesystem::path& path) const {
    auto info = probe(path);
    if (!info)
        return info;
    if (info->best_video) {
        VideoDecoderOptions options;
        options.paths = {DecodePath::Software};
        options.stream = *info->best_video;
        auto decoder = VideoDecoder::open(path, options);
        if (!decoder)
            return std::unexpected(decoder.error());
        auto first = (*decoder)->next();
        if (!first)
            return std::unexpected(first.error());
        if (!*first)
            return make_error(ErrorCode::InvalidData, Category::Decode,
                              "video has no decodable frame", path.string());
        if (!compositable((*first)->layout()))
            return make_error(ErrorCode::Unsupported, Category::Decode,
                              "video sample layout cannot be composited", path.string());
    }
    if (info->best_audio) {
        AudioDecoderOptions options;
        options.stream = *info->best_audio;
        auto decoder = AudioDecoder::open(path, options);
        if (!decoder)
            return std::unexpected(decoder.error());
        auto first = (*decoder)->next();
        if (!first)
            return std::unexpected(first.error());
        if (!*first)
            return make_error(ErrorCode::InvalidData, Category::Audio,
                              "audio has no decodable samples", path.string());
    }
    return info;
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
