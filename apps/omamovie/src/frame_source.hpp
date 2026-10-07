#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/media/probe.hpp"
#include "oma/media/video_decoder.hpp"
#include "oma/timeline/model.hpp"

#include <QImage>

#include <cstddef>
#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <string>

namespace oma::gpu {
class Device;
}

// A decoded picture and how to interpret it, ready to be a compositor layer input. The frame is
// shared: the decoder keeps it as its current frame while the viewer composites it.
struct Picture {
    std::shared_ptr<oma::media::VideoFrame> frame;
    oma::media::ColorInfo color;
    int rotation = 0; // counterclockwise display rotation of the stream
    oma::Rational sample_aspect = oma::Rational::literal(1, 1);
};

// Decodes the frame on screen at a media time, for the viewer and thumbnails. Without a device,
// software decode feeds the compositor's upload path. With a device, the render-thread pilot
// tries hardware decode first; thumbnails continue to use the CPU path.
//
// Threading: one owner thread per instance (session job worker or Qt render thread). Pictures
// handed out are only read elsewhere.
class FrameSource {
public:
    explicit FrameSource(const oma::gpu::Device* device = nullptr) : device_(device) {}
    // The frame whose PTS is the last one <= `t` (the first frame when `t` precedes it).
    // Moving forward a little reuses the open decoder without seeking, so playback decodes
    // each frame once. Moving back a little reads the frames decoded last; past them, the
    // decoder seeks a chunk earlier and decodes forward to `t`, keeping that chunk, so reverse
    // playback decodes each group of pictures about once instead of once per frame shown.
    [[nodiscard]] oma::Result<Picture> picture_at(const std::string& path, const oma::RationalTime& t);

    // A title clip's picture at the canvas size (ADR-0015), rasterized once per title and size:
    // the last kTitleCache are kept, most recent first, so playback does not redraw text.
    [[nodiscard]] oma::Result<Picture> title_picture(const oma::timeline::Title& title, std::uint32_t width,
                                                     std::uint32_t height);

    // The same frame converted to RGBA8 on the CPU (thumbnails, not the viewer).
    [[nodiscard]] oma::Result<QImage> image_at(const std::string& path, const oma::RationalTime& t);

private:
    const oma::gpu::Device* device_ = nullptr;
    struct TitleEntry {
        oma::timeline::Title title;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::shared_ptr<oma::media::VideoFrame> frame;
    };
    // A handful of titles on screen at once; a linear scan of a short list beats hashing text.
    static constexpr std::size_t kTitleCache = 8;
    std::deque<TitleEntry> titles_;
    struct Stream {
        std::string path;
        std::unique_ptr<oma::media::VideoDecoder> decoder;
        std::shared_ptr<oma::media::VideoFrame> current; // last frame returned (pts <= target)
        std::optional<oma::media::VideoFrame> ahead;     // the frame after it, if decoded
        bool ended = false;                              // no frame after `ahead`/`current`
        // The frames decoded since the last seek, in order, `current` last; trimmed from the
        // front to the byte budget shared by all streams, and to kStepBackFrames unless the
        // stream is stepping back (a backward chunk), so forward playback holds little memory.
        std::deque<std::shared_ptr<oma::media::VideoFrame>> history;
        bool backward = false;
    };

    // A decoder for `path` that reaches `t` cheapest: one that can decode forward to it, else a
    // new one while the file has fewer than kStreamsPerFile, else the least recently used.
    [[nodiscard]] oma::Result<Stream*> stream(const std::string& path, const oma::RationalTime& t);
    [[nodiscard]] static bool reaches(const Stream& s, const oma::RationalTime& t);
    [[nodiscard]] oma::Result<void> seek(Stream& s, const oma::RationalTime& t);
    [[nodiscard]] oma::Result<void> seek_to(Stream& s, const oma::RationalTime& t);
    [[nodiscard]] oma::Result<void> advance(Stream& s, const oma::RationalTime& t);
    // `frame` becomes the stream's current frame and the newest in its history.
    void record(Stream& s, std::shared_ptr<oma::media::VideoFrame> frame);
    // A frame already decoded that is the one on screen at `t` (a later frame follows it).
    [[nodiscard]] std::shared_ptr<oma::media::VideoFrame> remembered(const std::string& path,
                                                                     const oma::RationalTime& t) const;
    [[nodiscard]] Picture picture_of(const Stream& s, std::shared_ptr<oma::media::VideoFrame> frame) const;

    // A few open decoders, most recently used first: cutting between clips of the same files
    // keeps their decoders warm, and two per file let a transition between two parts of one
    // recording decode both forward instead of seeking each frame. Bounded so memory does not
    // grow with the library.
    static constexpr std::size_t kMaxStreams = 6;
    static constexpr std::size_t kStreamsPerFile = 2;
    std::list<Stream> streams_;
    // Decoded frames kept for stepping back, over all streams. ponytail: sized from the luma
    // plane times 1.5 (4:2:0), so 4:4:4 sources keep up to twice the budget.
    // At most 384 MB, and at most an eighth of the physical memory available when the first
    // source is created (M4: bounded by the machine, not only by a constant).
    [[nodiscard]] static std::size_t history_budget();
    static constexpr std::size_t kStepBackFrames = 8;
    std::size_t history_bytes_ = 0;
};
