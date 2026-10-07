#include "frame_source.hpp"

#include "title_raster.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <filesystem>
#include <span>
#include <utility>

#include <unistd.h>

namespace {

oma::RationalTime pts_of(const oma::media::VideoFrame& frame) {
    return frame.pts().value_or(oma::RationalTime{});
}

// How far ahead decoding forward beats seeking (which restarts at the previous keyframe).
constexpr double kForwardWindowSeconds = 2.0;
// The most frames one backward chunk decodes ahead of the target.
constexpr std::size_t kMaxChunkFrames = 64;

std::size_t bytes_of(const oma::media::VideoFrame& f) {
    const std::size_t sample = f.bit_depth() > 8 ? 2 : 1;
    return static_cast<std::size_t>(f.width()) * static_cast<std::size_t>(f.height()) * sample * 3 / 2;
}

} // namespace

std::size_t FrameSource::history_budget() {
    static const std::size_t budget = [] {
        constexpr std::size_t kMax = std::size_t{384} << 20;
        const long pages = ::sysconf(_SC_AVPHYS_PAGES);
        const long page = ::sysconf(_SC_PAGESIZE);
        if (pages <= 0 || page <= 0) return kMax;
        return std::min(kMax, static_cast<std::size_t>(pages) * static_cast<std::size_t>(page) / 8);
    }();
    return budget;
}

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
    options.device = device_;
    if (device_ == nullptr) options.paths = {oma::media::DecodePath::Software};
    auto decoder = oma::media::VideoDecoder::open(std::filesystem::path(path), options);
    if (!decoder) {
        return std::unexpected(decoder.error());
    }
    if (streams_.size() >= kMaxStreams) {
        for (const auto& f : streams_.back().history) history_bytes_ -= bytes_of(*f);
        streams_.pop_back();
    }
    streams_.push_front(Stream{
        .path = path, .decoder = std::move(*decoder), .current = {}, .ahead = {}, .ended = false, .history = {},
        .backward = false});
    return &streams_.front();
}

void FrameSource::record(Stream& s, std::shared_ptr<oma::media::VideoFrame> frame) {
    history_bytes_ += bytes_of(*frame);
    s.history.push_back(frame);
    s.current = std::move(frame);
    while (!s.backward && s.history.size() > kStepBackFrames) {
        history_bytes_ -= bytes_of(*s.history.front());
        s.history.pop_front();
    }
    // Over budget: drop the oldest frames, of the least recently used streams first. Never
    // the current frame of a stream (it is still its decoder's position).
    for (auto it = streams_.rbegin(); it != streams_.rend() && history_bytes_ > history_budget(); ++it) {
        while (it->history.size() > 1 && history_bytes_ > history_budget()) {
            history_bytes_ -= bytes_of(*it->history.front());
            it->history.pop_front();
        }
    }
}

std::shared_ptr<oma::media::VideoFrame> FrameSource::remembered(const std::string& path,
                                                                const oma::RationalTime& t) const {
    for (const Stream& s : streams_) {
        if (s.path != path) continue;
        // O(log h): the first remembered frame after `t`; the one before it is on screen at t.
        const auto after = std::ranges::upper_bound(s.history, t, std::less{},
                                                    [](const auto& f) { return pts_of(*f); });
        if (after != s.history.begin() && after != s.history.end()) return *std::prev(after);
    }
    return nullptr;
}

oma::Result<void> FrameSource::seek(Stream& s, const oma::RationalTime& t) {
    for (const auto& f : s.history) history_bytes_ -= bytes_of(*f);
    s.history.clear(); // a new run: the history stays contiguous
    s.backward = s.current && t < pts_of(*s.current);
    // Stepping back from where this decoder is: start a chunk earlier and decode forward to `t`,
    // so the next steps back find their frames in the history.
    if (s.backward) {
        const std::size_t frame_bytes = std::max<std::size_t>(1, bytes_of(*s.current));
        const std::size_t frames = std::clamp<std::size_t>(history_budget() / 2 / frame_bytes, 1, kMaxChunkFrames);
        // The nominal frame rate only sizes the chunk; frames are still found by PTS.
        const auto& video = s.decoder->stream().video;
        const oma::Rational frame = video && video->frame_rate ? video->frame_rate->frame_duration()
                                                               : oma::Rational::literal(1, 30);
        const auto back = oma::rescale(static_cast<std::int64_t>(frames), frame, t.timebase(), oma::Rounding::Floor);
        if (auto earlier = back ? oma::RationalTime::make(t.value() - *back, t.timebase())
                                : oma::Result<oma::RationalTime>(std::unexpected(back.error()));
            earlier && *back > 0) {
            if (auto r = seek_to(s, *earlier); !r) return r;
            return advance(s, t);
        }
    }
    return seek_to(s, t);
}

oma::Result<void> FrameSource::seek_to(Stream& s, const oma::RationalTime& t) {
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
    record(s, std::make_shared<oma::media::VideoFrame>(std::move(**first)));
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
        record(s, std::make_shared<oma::media::VideoFrame>(std::move(*s.ahead)));
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
    if (auto frame = remembered(path, t)) {
        const auto s = std::ranges::find(streams_, path, &Stream::path);
        return picture_of(*s, std::move(frame));
    }
    auto opened = stream(path, t);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    Stream& s = **opened;
    const bool forward = reaches(s, t);
    if (forward) s.backward = false; // playing on: back to a short history
    if (auto r = forward ? advance(s, t) : seek(s, t); !r) {
        return std::unexpected(r.error());
    }
    return picture_of(s, s.current);
}

Picture FrameSource::picture_of(const Stream& s, std::shared_ptr<oma::media::VideoFrame> frame) const {
    Picture picture{.frame = std::move(frame), .color = {}, .rotation = 0, .sample_aspect = oma::Rational::literal(1, 1)};
    if (const auto& video = s.decoder->stream().video) {
        picture.color = video->color;
        picture.rotation = video->rotation;
        picture.sample_aspect = video->sample_aspect;
    }
    return picture;
}

oma::Result<Picture> FrameSource::title_picture(const oma::timeline::Title& title, std::uint32_t width,
                                               std::uint32_t height) {
    const auto it = std::ranges::find_if(titles_, [&](const TitleEntry& e) {
        return e.width == width && e.height == height && e.title == title;
    });
    if (it != titles_.end()) {
        if (it != titles_.begin()) std::rotate(titles_.begin(), it, std::next(it));
    } else {
        auto frame = rasterize_title(title, width, height);
        if (!frame) return std::unexpected(frame.error());
        titles_.push_front({.title = title,
                            .width = width,
                            .height = height,
                            .frame = std::make_shared<oma::media::VideoFrame>(std::move(*frame))});
        if (titles_.size() > kTitleCache) titles_.pop_back();
    }
    // Drawn in sRGB, full range (ADR-0006 reads RGB sources as sRGB).
    return Picture{.frame = titles_.front().frame, .color = {}, .rotation = 0,
                   .sample_aspect = oma::Rational::literal(1, 1)};
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
