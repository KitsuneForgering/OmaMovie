#include "oma/media/video_decoder.hpp"

#include "media_test.hpp"

#include <string>
#include <vector>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::Rational;
using oma::RationalTime;
using oma::media::DecodePath;
using oma::media::VideoDecoder;
using oma::media::VideoDecoderOptions;
using oma::media::VideoFrame;

namespace {

VideoDecoderOptions software_only() {
    VideoDecoderOptions o;
    o.paths = {DecodePath::Software};
    return o;
}

std::unique_ptr<VideoDecoder> open_software(const char* name) {
    auto d = VideoDecoder::open(fixture(name), software_only());
    expect(d.has_value()).toBeTruthy();
    return d ? std::move(*d) : nullptr;
}

// Presentation timestamps of every frame, in stream ticks.
std::vector<std::int64_t> all_pts(VideoDecoder& d) {
    std::vector<std::int64_t> out;
    for (;;) {
        auto f = d.next();
        if (!f || !*f) {
            break;
        }
        out.push_back((*f)->pts().value_or(RationalTime()).value());
    }
    return out;
}

RationalTime ticks(const VideoDecoder& d, std::int64_t value) {
    return RationalTime::make(value, d.stream().timebase).value();
}

void decodes_every_frame() {
    auto d = open_software("h264_30fps_aac.mp4");
    if (!d) {
        return;
    }
    expect(d->path() == DecodePath::Software).toBeTruthy();
    expect(d->stream().codec).toEqual("h264");
    int count = 0;
    std::int64_t last = -1;
    bool increasing = true;
    for (;;) {
        auto f = d->next();
        expect(f.has_value()).toBeTruthy();
        if (!f || !*f) {
            break;
        }
        VideoFrame& frame = **f;
        if (count == 0) {
            expect(frame.on_gpu()).toBeFalsy();
            expect(frame.width()).toEqual(320);
            expect(frame.height()).toEqual(180);
            expect(std::string(frame.pixel_format())).toEqual("yuv420p");
            expect(frame.plane_count()).toEqual(3);
            expect(frame.stride(0) >= 320).toBeTruthy();
            expect(frame.plane(0).size() >= static_cast<std::size_t>(frame.stride(0)) * 180)
                .toBeTruthy();
            expect(frame.pts().has_value()).toBeTruthy();
            expect(frame.pts()->value()).toEqual(0);
            expect(frame.acquire_gpu().has_value()).toBeFalsy();
        }
        const std::int64_t pts = frame.pts().value_or(RationalTime()).value();
        increasing = increasing && pts > last;
        last = pts;
        ++count;
    }
    expect(count).toEqual(30);
    expect(increasing).toBeTruthy();
    auto after_end = d->next();
    expect(after_end.has_value() && !after_end->has_value()).toBeTruthy();
}

void ten_bit_software() {
    if (!have_fixture("hevc_10bit.mp4")) {
        return;
    }
    auto d = open_software("hevc_10bit.mp4");
    if (!d) {
        return;
    }
    auto f = d->next();
    expect(f.has_value() && f->has_value()).toBeTruthy();
    if (f && *f) {
        expect(std::string((*f)->pixel_format())).toEqual("yuv420p10le");
        expect((*f)->bit_depth()).toEqual(10);
    }
}

// On VFR media the frame on screen at t is the last one with pts <= t.
void seeks_exactly_on_vfr() {
    auto d = open_software("h264_vfr.mkv");
    if (!d) {
        return;
    }
    const std::vector<std::int64_t> pts = all_pts(*d);
    expect(pts.size() > 10).toBeTruthy();
    if (pts.size() <= 10) {
        return;
    }
    // Between two frames, inside a long (1/15 s) gap, in both directions.
    for (const std::size_t i : {std::size_t{7}, std::size_t{2}, pts.size() - 3}) {
        const std::int64_t target = pts[i] + ((pts[i + 1] - pts[i]) / 2);
        expect(d->seek(ticks(*d, target)).has_value()).toBeTruthy();
        auto f = d->next();
        expect(f.has_value() && f->has_value()).toBeTruthy();
        if (f && *f) {
            expect((*f)->pts()->value()).toEqual(pts[i]);
        }
        auto following = d->next();
        expect(following.has_value() && following->has_value()).toBeTruthy();
        if (following && *following) {
            expect((*following)->pts()->value()).toEqual(pts[i + 1]);
        }
    }
}

void seek_edges() {
    auto d = open_software("h264_vfr.mkv");
    if (!d) {
        return;
    }
    const std::vector<std::int64_t> pts = all_pts(*d);
    if (pts.empty()) {
        return;
    }
    // Exactly on a frame.
    expect(d->seek(ticks(*d, pts[5])).has_value()).toBeTruthy();
    auto on = d->next();
    expect(on && *on && (*on)->pts()->value() == pts[5]).toBeTruthy();
    // Before the first frame: the first frame.
    expect(d->seek(ticks(*d, -1000)).has_value()).toBeTruthy();
    auto first = d->next();
    expect(first && *first && (*first)->pts()->value() == pts.front()).toBeTruthy();
    // Past the end: the last frame, then the end of the stream.
    expect(d->seek(ticks(*d, pts.back() + 10000)).has_value()).toBeTruthy();
    auto last = d->next();
    expect(last && *last && (*last)->pts()->value() == pts.back()).toBeTruthy();
    auto end = d->next();
    expect(end.has_value() && !end->has_value()).toBeTruthy();
}

void seek_across_timebases() {
    auto d = open_software("h264_29.97fps.mp4");
    if (!d) {
        return;
    }
    // Frame 15 at 30000/1001 fps, given in that rate's own timebase rather than the stream's.
    const auto t = RationalTime::make(15 * 1001, Rational::literal(1, 30000)).value();
    expect(d->seek(t).has_value()).toBeTruthy();
    auto f = d->next();
    expect(f && *f && (*f)->pts().has_value()).toBeTruthy();
    if (f && *f) {
        expect(*(*f)->pts() == t).toBeTruthy();
    }
}

void no_path_without_device() {
    VideoDecoderOptions o;
    o.paths = {DecodePath::VaapiToVulkan, DecodePath::VulkanVideo};
    auto d = VideoDecoder::open(fixture("h264_30fps_aac.mp4"), o);
    expect(d.has_value()).toBeFalsy();
    if (!d) {
        expect(code_of(d.error())).toEqual(static_cast<int>(ErrorCode::Unsupported));
    }
}

void rejects_bad_input() {
    auto truncated = VideoDecoder::open(fixture("truncated.mp4"), software_only());
    expect(truncated.has_value()).toBeFalsy();
    VideoDecoderOptions audio_stream = software_only();
    audio_stream.stream = 1; // the AAC stream
    auto wrong = VideoDecoder::open(fixture("h264_30fps_aac.mp4"), audio_stream);
    expect(wrong.has_value()).toBeFalsy();
    if (!wrong) {
        expect(code_of(wrong.error())).toEqual(static_cast<int>(ErrorCode::InvalidArgument));
    }
}

} // namespace

void run_video_decoder_tests() {
    describe("media::VideoDecoder (software)", {
        it("decodes every frame in presentation order", { decodes_every_frame(); });
        it("keeps 10-bit samples", { ten_bit_software(); });
        it("seeks to the frame on screen on VFR media", { seeks_exactly_on_vfr(); });
        it("seeks to exact frames, before the start and past the end", { seek_edges(); });
        it("seeks with a time in another timebase", { seek_across_timebases(); });
        it("fails without a usable path", { no_path_without_device(); });
        it("rejects damaged files and non-video streams", { rejects_bad_input(); });
    });
}
