#include "oma/media/probe.hpp"

#include "media_test.hpp"

#include <string>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::Rational;
using oma::media::ColorRange;
using oma::media::MediaInfo;
using oma::media::probe;
using oma::media::StreamInfo;
using oma::media::StreamKind;

namespace {

const StreamInfo* best_video(const MediaInfo& m) {
    return m.best_video ? &m.streams[static_cast<std::size_t>(*m.best_video)] : nullptr;
}

const StreamInfo* best_audio(const MediaInfo& m) {
    return m.best_audio ? &m.streams[static_cast<std::size_t>(*m.best_audio)] : nullptr;
}

void h264_aac() {
    auto info = probe(fixture("h264_30fps_aac.mp4"));
    expect(info.has_value()).toBeTruthy();
    if (!info) {
        return;
    }
    expect(info->container).toContain("mp4");
    expect(info->streams.size()).toEqual(2);
    expect(info->duration.has_value()).toBeTruthy();
    expect(info->duration->seconds_approx()).toBeCloseTo(1.0, 0.1);

    const StreamInfo* v = best_video(*info);
    expect(v != nullptr).toBeTruthy();
    if (v == nullptr) {
        return;
    }
    expect(v->kind == StreamKind::Video).toBeTruthy();
    expect(v->codec).toEqual("h264");
    expect(v->video->width).toEqual(320);
    expect(v->video->height).toEqual(180);
    expect(v->video->pixel_format).toEqual("yuv420p");
    expect(v->video->bit_depth).toEqual(8);
    expect(v->video->rotation).toEqual(0);
    expect(v->video->frame_rate.has_value()).toBeTruthy();
    expect(v->video->frame_rate->fps() == Rational::literal(30, 1)).toBeTruthy();
    expect(v->video->variable_frame_rate).toBeFalsy();
    expect(v->video->still_image).toBeFalsy();
    expect(v->frame_count.value_or(0)).toEqual(30);

    const StreamInfo* a = best_audio(*info);
    expect(a != nullptr).toBeTruthy();
    if (a == nullptr) {
        return;
    }
    expect(a->codec).toEqual("aac");
    expect(a->audio->sample_rate.has_value()).toBeTruthy();
    expect(a->audio->sample_rate->hz()).toEqual(48000);
    expect(a->audio->channels).toEqual(1);
    expect(a->audio->channel_layout).toEqual("mono");
}

void ntsc_rate() {
    auto info = probe(fixture("h264_29.97fps.mp4"));
    expect(info.has_value()).toBeTruthy();
    if (!info || best_video(*info) == nullptr) {
        return;
    }
    const auto& rate = best_video(*info)->video->frame_rate;
    expect(rate.has_value()).toBeTruthy();
    expect(rate && rate->fps() == Rational::literal(30000, 1001)).toBeTruthy();
}

void variable_rate() {
    auto info = probe(fixture("h264_vfr.mkv"));
    expect(info.has_value()).toBeTruthy();
    if (!info || best_video(*info) == nullptr) {
        return;
    }
    expect(best_video(*info)->video->variable_frame_rate).toBeTruthy();
}

void rotation() {
    auto info = probe(fixture("h264_rotated90.mp4"));
    expect(info.has_value()).toBeTruthy();
    if (!info || best_video(*info) == nullptr) {
        return;
    }
    expect(best_video(*info)->video->rotation).toEqual(90);
}

void ten_bit() {
    if (!have_fixture("hevc_10bit.mp4")) {
        return;
    }
    auto info = probe(fixture("hevc_10bit.mp4"));
    expect(info.has_value()).toBeTruthy();
    if (!info || best_video(*info) == nullptr) {
        return;
    }
    const StreamInfo& v = *best_video(*info);
    expect(v.codec).toEqual("hevc");
    expect(v.profile).toEqual("Main 10");
    expect(v.video->bit_depth).toEqual(10);
    expect(v.video->color.range == ColorRange::Full).toBeFalsy();
}

void audio_only() {
    auto info = probe(fixture("tone_44100.wav"));
    expect(info.has_value()).toBeTruthy();
    if (!info) {
        return;
    }
    expect(info->best_video.has_value()).toBeFalsy();
    const StreamInfo* a = best_audio(*info);
    expect(a != nullptr).toBeTruthy();
    if (a != nullptr) {
        expect(a->audio->sample_rate->hz()).toEqual(44100);
        expect(a->duration.has_value()).toBeTruthy();
        expect(a->duration->seconds_approx()).toBeCloseTo(2.0, 0.05);
    }
}

void still_image() {
    auto info = probe(fixture("still.png"));
    expect(info.has_value()).toBeTruthy();
    if (!info || best_video(*info) == nullptr) {
        return;
    }
    expect(best_video(*info)->video->still_image).toBeTruthy();
    expect(best_video(*info)->video->width).toEqual(640);
}

void truncated() {
    auto info = probe(fixture("truncated.mp4"));
    expect(info.has_value()).toBeFalsy();
    if (!info) {
        expect(code_of(info.error())).toEqual(static_cast<int>(ErrorCode::InvalidData));
    }
}

void missing() {
    auto info = probe(fixture("does-not-exist.mp4"));
    expect(info.has_value()).toBeFalsy();
    if (!info) {
        expect(code_of(info.error())).toEqual(static_cast<int>(ErrorCode::IoError));
        expect(info.error().context()).toContain("does-not-exist.mp4");
    }
}

} // namespace

void run_probe_tests() {
    describe("media::probe", {
        it("describes an H.264 + AAC file", { h264_aac(); });
        it("keeps NTSC frame rates exact", { ntsc_rate(); });
        it("detects variable frame rate from timestamps", { variable_rate(); });
        it("reads the display rotation", { rotation(); });
        it("reports 10-bit HEVC", { ten_bit(); });
        it("describes audio-only files", { audio_only(); });
        it("marks still images", { still_image(); });
        it("rejects a truncated file as invalid data", { truncated(); });
        it("reports a missing file as an I/O error", { missing(); });
    });
}
