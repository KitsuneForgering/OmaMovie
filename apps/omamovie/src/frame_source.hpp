#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/media/probe.hpp"
#include "oma/media/video_decoder.hpp"

#include <QImage>

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <string>

// A decoded picture and how to interpret it, ready to be a compositor layer input. The frame is
// shared: the decoder keeps it as its current frame while the viewer composites it.
struct Picture {
    std::shared_ptr<oma::media::VideoFrame> frame;
    oma::media::ColorInfo color;
    int rotation = 0; // counterclockwise display rotation of the stream
    oma::Rational sample_aspect = oma::Rational::literal(1, 1);
};

// Decodes the frame on screen at a media time, for the viewer and thumbnails. Software decode
// for now: the viewer's compositor uploads the planes once (CLAUDE.md §7.3 fallback). Hardware
// frames need the playback scheduler's queue admission first (ADR-0005).
//
// Threading: owned by the session's single job worker; every call happens on that thread
// (jobs on a one-thread JobPool never overlap). Pictures handed out are only read elsewhere.
class FrameSource {
public:
    // The frame whose PTS is the last one <= `t` (the first frame when `t` precedes it).
    // Moving forward a little reuses the open decoder without seeking, so playback decodes
    // each frame once.
    [[nodiscard]] oma::Result<Picture> picture_at(const std::string& path, const oma::RationalTime& t);

    // The same frame converted to RGBA8 on the CPU (thumbnails, not the viewer).
    [[nodiscard]] oma::Result<QImage> image_at(const std::string& path, const oma::RationalTime& t);

private:
    struct Stream {
        std::string path;
        std::unique_ptr<oma::media::VideoDecoder> decoder;
        std::shared_ptr<oma::media::VideoFrame> current; // last frame returned (pts <= target)
        std::optional<oma::media::VideoFrame> ahead;     // the frame after it, if decoded
        bool ended = false;                              // no frame after `ahead`/`current`
    };

    [[nodiscard]] oma::Result<Stream*> stream(const std::string& path);
    [[nodiscard]] static oma::Result<void> seek(Stream& s, const oma::RationalTime& t);
    [[nodiscard]] static oma::Result<void> advance(Stream& s, const oma::RationalTime& t);

    // A few open decoders, most recently used first: cutting between clips of the same files
    // keeps their decoders warm. Bounded so memory does not grow with the library.
    static constexpr std::size_t kMaxStreams = 4;
    std::list<Stream> streams_;
};
