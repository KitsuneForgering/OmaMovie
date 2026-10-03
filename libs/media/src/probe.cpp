#include "oma/media/probe.hpp"

#include "ffmpeg.hpp"
#include "stream_info.hpp"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/display.h>
#include <libavutil/pixdesc.h>
}

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

namespace oma::media {

namespace {

// Packets read to judge whether the frame rate is constant.
constexpr int kVfrSamplePackets = 48;

StreamKind to_kind(AVMediaType type) noexcept {
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        return StreamKind::Video;
    case AVMEDIA_TYPE_AUDIO:
        return StreamKind::Audio;
    case AVMEDIA_TYPE_SUBTITLE:
        return StreamKind::Subtitle;
    case AVMEDIA_TYPE_DATA:
        return StreamKind::Data;
    case AVMEDIA_TYPE_ATTACHMENT:
        return StreamKind::Attachment;
    default:
        return StreamKind::Unknown;
    }
}

ColorRange to_range(AVColorRange range) noexcept {
    switch (range) {
    case AVCOL_RANGE_MPEG:
        return ColorRange::Limited;
    case AVCOL_RANGE_JPEG:
        return ColorRange::Full;
    default:
        return ColorRange::Unspecified;
    }
}

int rotation_of(const AVCodecParameters& par) {
    const AVPacketSideData* sd = av_packet_side_data_get(
        par.coded_side_data, par.nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX);
    if (sd == nullptr || sd->size < 9 * sizeof(int32_t)) {
        return 0;
    }
    const double degrees = av_display_rotation_get(reinterpret_cast<const int32_t*>(sd->data));
    if (std::isnan(degrees)) {
        return 0;
    }
    // Snap to quarter turns and normalize to [0, 360).
    const long quarter = std::lround(degrees / 90.0);
    return static_cast<int>(((quarter % 4) + 4) % 4) * 90;
}

std::string channel_layout_name(const AVChannelLayout& layout) {
    std::array<char, 128> buf{};
    if (av_channel_layout_describe(&layout, buf.data(), buf.size()) < 0) {
        return {};
    }
    return buf.data();
}

std::string dict_value(const AVDictionary* dict, const char* key) {
    const AVDictionaryEntry* e = av_dict_get(dict, key, nullptr, 0);
    return e != nullptr ? e->value : std::string();
}

bool is_image_demuxer(std::string_view name) {
    return name == "image2" || name.ends_with("_pipe");
}

VideoInfo video_info(const AVStream& st, std::string_view demuxer) {
    const AVCodecParameters& par = *st.codecpar;
    VideoInfo v;
    v.width = par.width;
    v.height = par.height;
    const auto format = static_cast<AVPixelFormat>(par.format);
    if (const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format); desc != nullptr) {
        v.pixel_format = desc->name;
        v.bit_depth = desc->comp[0].depth;
    }
    v.color = {.matrix = static_cast<std::uint8_t>(par.color_space),
               .primaries = static_cast<std::uint8_t>(par.color_primaries),
               .transfer = static_cast<std::uint8_t>(par.color_trc),
               .range = to_range(par.color_range)};
    v.rotation = rotation_of(par);
    if (auto sar = ff::to_rational(par.sample_aspect_ratio); sar && sar->is_positive()) {
        v.sample_aspect = *sar;
    }
    for (const AVRational r : {st.avg_frame_rate, st.r_frame_rate}) {
        if (auto q = ff::to_rational(r); q && q->is_positive()) {
            if (auto fr = FrameRate::make(*q)) {
                v.frame_rate = *fr;
                break;
            }
        }
    }
    v.still_image = (static_cast<unsigned>(st.disposition) &
                     static_cast<unsigned>(AV_DISPOSITION_ATTACHED_PIC)) != 0 ||
                    is_image_demuxer(demuxer);
    return v;
}

AudioInfo audio_info(const AVStream& st) {
    const AVCodecParameters& par = *st.codecpar;
    AudioInfo a;
    if (auto rate = SampleRate::make(par.sample_rate)) {
        a.sample_rate = *rate;
    }
    a.channels = par.ch_layout.nb_channels;
    a.channel_layout = channel_layout_name(par.ch_layout);
    if (const char* name = av_get_sample_fmt_name(static_cast<AVSampleFormat>(par.format))) {
        a.sample_format = name;
    }
    return a;
}

// Reads the first packets of `stream` and reports whether their spacing varies. Containers often
// claim a constant average rate for VFR recordings, so the timestamps are the only evidence.
bool spacing_varies(AVFormatContext& fmt, int stream) {
    std::vector<std::int64_t> pts;
    ff::PacketPtr pkt(av_packet_alloc());
    if (!pkt) {
        return false;
    }
    int read = 0;
    while (pts.size() < kVfrSamplePackets && read < kVfrSamplePackets * 8 &&
           av_read_frame(&fmt, pkt.get()) >= 0) {
        ++read;
        if (pkt->stream_index == stream && pkt->pts != AV_NOPTS_VALUE) {
            pts.push_back(pkt->pts);
        }
        av_packet_unref(pkt.get());
    }
    if (pts.size() < 3) {
        return false;
    }
    std::ranges::sort(pts); // decode order differs from presentation order with B-frames
    std::int64_t lo = INT64_MAX;
    std::int64_t hi = 0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const std::int64_t d = pts[i] - pts[i - 1];
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    // Rounding to a coarse timebase (1/1000 in Matroska) jitters CFR spacing by one tick.
    return hi - lo > 1 && (hi - lo) * 10 > lo;
}

} // namespace

std::string_view to_string(StreamKind kind) noexcept {
    switch (kind) {
    case StreamKind::Video:
        return "video";
    case StreamKind::Audio:
        return "audio";
    case StreamKind::Subtitle:
        return "subtitle";
    case StreamKind::Data:
        return "data";
    case StreamKind::Attachment:
        return "attachment";
    case StreamKind::Unknown:
        break;
    }
    return "unknown";
}

StreamInfo describe_stream(const AVStream& st, std::string_view demuxer) {
    const AVCodecParameters& par = *st.codecpar;
    StreamInfo s;
    s.index = st.index;
    s.kind = to_kind(par.codec_type);
    s.codec = avcodec_get_name(par.codec_id);
    if (const char* profile = avcodec_profile_name(par.codec_id, par.profile)) {
        s.profile = profile;
    }
    if (auto tb = ff::to_timebase(st.time_base)) {
        s.timebase = *tb;
        s.start = ff::to_time(st.start_time, *tb);
        s.duration = ff::to_time(st.duration, *tb);
    }
    if (st.nb_frames > 0) {
        s.frame_count = st.nb_frames;
    }
    s.language = dict_value(st.metadata, "language");
    if (s.kind == StreamKind::Video) {
        s.video = video_info(st, demuxer);
    } else if (s.kind == StreamKind::Audio) {
        s.audio = audio_info(st);
    }
    return s;
}

Result<MediaInfo> probe(const std::filesystem::path& path) {
    auto opened = ff::open_input(path);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    AVFormatContext& fmt = **opened;

    MediaInfo info;
    info.container = fmt.iformat->name;
    info.duration = ff::to_time(fmt.duration, Rational::literal(1, AV_TIME_BASE));
    info.bit_rate = fmt.bit_rate;

    for (unsigned i = 0; i < fmt.nb_streams; ++i) {
        info.streams.push_back(describe_stream(*fmt.streams[i], info.container));
    }

    if (const int v = av_find_best_stream(&fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0); v >= 0) {
        info.best_video = v;
        auto& video = info.streams[static_cast<std::size_t>(v)].video;
        if (video && !video->still_image) {
            video->variable_frame_rate = spacing_varies(fmt, v);
        }
    }
    if (const int a = av_find_best_stream(&fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0); a >= 0) {
        info.best_audio = a;
    }
    return info;
}

} // namespace oma::media
