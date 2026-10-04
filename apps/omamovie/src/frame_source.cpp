#include "frame_source.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <span>
#include <utility>

namespace {

oma::RationalTime pts_of(const oma::media::VideoFrame& frame) {
    return frame.pts().value_or(oma::RationalTime{});
}

// How far ahead decoding forward beats seeking (which restarts at the previous keyframe).
constexpr double kForwardWindowSeconds = 2.0;

} // namespace

bool FrameSource::reaches(const Stream& s, const oma::RationalTime& t) {
    // Display-grade seconds only choose between decoding forward and seeking.
    return s.current && pts_of(*s.current) <= t &&
           (t.seconds_approx() - pts_of(*s.current).seconds_approx()) <= kForwardWindowSeconds;
}

oma::Result<FrameSource::Stream*> FrameSource::stream(const std::string& path, const oma::RationalTime& t) {
    auto it = std::ranges::find_if(streams_, [&](const Stream& s) { return s.path == path && reaches(s, t); });
    const auto open = std::ranges::count(streams_, path, &Stream::path);
    if (it == streams_.end() && std::cmp_greater_equal(open, kStreamsPerFile)) {
        // Seek the least recently used decoder of this file.
        it = std::ranges::find(streams_.rbegin(), streams_.rend(), path, &Stream::path).base();
        --it;
    }
    if (it != streams_.end()) {
        streams_.splice(streams_.begin(), streams_, it);
        return &streams_.front();
    }
    oma::media::VideoDecoderOptions options;
    options.paths = {oma::media::DecodePath::Software};
    auto decoder = oma::media::VideoDecoder::open(std::filesystem::path(path), options);
    if (!decoder) {
        return std::unexpected(decoder.error());
    }
    if (streams_.size() >= kMaxStreams) {
        streams_.pop_back();
    }
    streams_.push_front(
        Stream{.path = path, .decoder = std::move(*decoder), .current = {}, .ahead = {}, .ended = false});
    return &streams_.front();
}

oma::Result<void> FrameSource::seek(Stream& s, const oma::RationalTime& t) {
    if (auto r = s.decoder->seek(t); !r) {
        return r;
    }
    auto first = s.decoder->next();
    if (!first) {
        return std::unexpected(first.error());
    }
    if (!*first) {
        return oma::make_error(oma::ErrorCode::OutOfRange, oma::Category::Playback, "no frame at this time",
                               s.path);
    }
    s.current = std::make_shared<oma::media::VideoFrame>(std::move(**first));
    auto next = s.decoder->next();
    if (!next) {
        return std::unexpected(next.error());
    }
    s.ahead = std::move(*next);
    s.ended = !s.ahead;
    return {};
}

oma::Result<void> FrameSource::advance(Stream& s, const oma::RationalTime& t) {
    while (s.ahead && pts_of(*s.ahead) <= t) {
        s.current = std::make_shared<oma::media::VideoFrame>(std::move(*s.ahead));
        auto next = s.decoder->next();
        if (!next) {
            return std::unexpected(next.error());
        }
        s.ahead = std::move(*next);
        s.ended = !s.ahead;
    }
    return {};
}

oma::Result<Picture> FrameSource::picture_at(const std::string& path, const oma::RationalTime& t) {
    auto opened = stream(path, t);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    Stream& s = **opened;
    if (auto r = reaches(s, t) ? advance(s, t) : seek(s, t); !r) {
        return std::unexpected(r.error());
    }
    Picture picture{.frame = s.current, .color = {}, .rotation = 0, .sample_aspect = oma::Rational::literal(1, 1)};
    if (const auto& video = s.decoder->stream().video) {
        picture.color = video->color;
        picture.rotation = video->rotation;
        picture.sample_aspect = video->sample_aspect;
    }
    return picture;
}

oma::Result<QImage> FrameSource::image_at(const std::string& path, const oma::RationalTime& t) {
    auto picture = picture_at(path, t);
    if (!picture) {
        return std::unexpected(picture.error());
    }
    const oma::media::VideoFrame& frame = *picture->frame;
    QImage image(frame.width(), frame.height(), QImage::Format_RGBA8888);
    if (image.isNull()) {
        return oma::make_error(oma::ErrorCode::Internal, oma::Category::Ui, "cannot allocate the thumbnail");
    }
    auto copied =
        frame.copy_rgba({image.bits(), static_cast<std::size_t>(image.sizeInBytes())}, image.bytesPerLine());
    if (!copied) {
        return std::unexpected(copied.error());
    }
    return image;
}
