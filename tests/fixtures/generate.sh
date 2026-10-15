#!/usr/bin/env bash
# Generates small, reproducible test media in tests/fixtures/generated/ (not versioned).
# Usage: make fixtures   (or tests/fixtures/generate.sh [directory])
#
# Covers the cases from CLAUDE.md §6 and §25: CFR and VFR, NTSC rates, 8 and 10 bits,
# H.264/HEVC/AV1, AAC/Opus, rotation, a still image and a truncated file. Codecs missing from
# the local FFmpeg are skipped with a notice. No copyrighted media: everything comes from lavfi.

set -euo pipefail

out="${1:-$(dirname "$0")/generated}"
mkdir -p "$out"

if ! command -v ffmpeg >/dev/null; then
    echo "ffmpeg not found" >&2
    exit 1
fi

export SVT_LOG=1 # SVT-AV1: errors only
ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }
# Read once: with pipefail, `ffmpeg | grep -q` would fail on SIGPIPE when grep exits early.
encoders="$(ffmpeg -hide_banner -encoders 2>/dev/null)"
has_encoder() { grep -qE "^ [A-Z.]{6} $1 " <<<"$encoders"; }

# 1 s of test video + a 440 Hz tone.
video_src() { echo "testsrc2=size=320x180:rate=$1:duration=${2:-1}"; }
audio_src() { echo "sine=frequency=440:sample_rate=${1:-48000}:duration=${2:-1}"; }

made=()
skipped=()
note() { made+=("$1"); }
skip() { skipped+=("$1 ($2)"); }

# H.264 CFR, 30 fps, AAC 48 kHz — the base case.
if has_encoder libx264; then
    ff -f lavfi -i "$(video_src 30)" -f lavfi -i "$(audio_src 48000)" \
        -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac -shortest "$out/h264_30fps_aac.mp4"
    note h264_30fps_aac.mp4

    # The same picture with sound whose loudness steps up each video frame (frame k plays
    # 0.9·(k+1)/31 of full scale), so a decoded window of audio names its source frame.
    ff -f lavfi -i "$(video_src 30)" \
        -f lavfi -i "aevalsrc='0.9*(floor(t*30)+1)/31*sin(2*PI*440*t)':s=48000:d=1" \
        -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a pcm_s16le -shortest \
        "$out/h264_30fps_steps.mkv"
    note h264_30fps_steps.mkv

    # Shaped like an omarchy-capture-screenrecording file (gpu-screen-recorder, probed on real
    # recordings 2026-10-13): H.264 High 1080p60 CFR without B-frames, time base 1/15360, BT.709
    # limited range, and AAC LC stereo at 96 kHz running 0.1 s past the video. Frame k plays a
    # tone of 120·(k+4) Hz: whole cycles in any 1/120 s window, so the strongest DFT bin of
    # decoded audio names its source frame whatever gain the codec adds (loudness steps of 1/67
    # did not survive AAC). Real recordings may have no sound at all: the silent copy covers that.
    recorder=(-c:v libx264 -preset veryfast -profile:v high -bf 0 -pix_fmt yuv420p
        -color_range tv -colorspace bt709 -color_primaries bt709 -color_trc bt709
        -video_track_timescale 15360)
    ff -f lavfi -i "testsrc2=size=1920x1080:rate=60:duration=1" \
        -f lavfi -i "aevalsrc='0.5*sin(2*PI*120*(floor(t*60)+4)*t)|0.5*sin(2*PI*120*(floor(t*60)+4)*t)':s=96000:d=1.1" \
        "${recorder[@]}" -c:a aac -b:a 256k "$out/screenrecording_like.mp4"
    note screenrecording_like.mp4
    ff -f lavfi -i "testsrc2=size=1920x1080:rate=60:duration=1" "${recorder[@]}" \
        "$out/screenrecording_silent.mp4"
    note screenrecording_silent.mp4

    # A gray ramp over every limited-range code (16..235 left to right), losslessly coded: an
    # ungraded export must bring each code back (ADR-0006 export transfer).
    ff -f lavfi -i "nullsrc=size=320x180:rate=30:duration=1,format=yuv420p,geq=lum='16+floor(X*220/320)':cb=128:cr=128" \
        -c:v libx264 -preset veryfast -qp 0 \
        -color_range tv -colorspace bt709 -color_primaries bt709 -color_trc bt709 \
        "$out/gray_ramp_bt709.mp4"
    note gray_ramp_bt709.mp4

    # NTSC rates (exact fractions).
    ff -f lavfi -i "$(video_src 30000/1001)" -c:v libx264 -preset veryfast -pix_fmt yuv420p \
        "$out/h264_29.97fps.mp4"
    note h264_29.97fps.mp4
    # One keyframe for 10 s: a seek decodes up to 300 frames forward, long enough for a newer
    # request to supersede it (scrubbing tests).
    ff -f lavfi -i "$(video_src 30 10)" -c:v libx264 -preset veryfast -pix_fmt yuv420p \
        -x264-params keyint=300:min-keyint=300:scenecut=0 "$out/h264_long_gop.mp4"
    note h264_long_gop.mp4
    ff -f lavfi -i "$(video_src 24000/1001)" -c:v libx264 -preset veryfast -pix_fmt yuv420p \
        "$out/h264_23.976fps.mp4"
    note h264_23.976fps.mp4

    # VFR: dropping every third frame alternates 1/30 s and 1/15 s gaps between frames. (Shifting
    # timestamps by half a frame does not work: the encoder timebase is 1/rate and rounds them.)
    ff -f lavfi -i "$(video_src 30 2)" \
        -vf "select='not(eq(mod(n\\,3)\\,1))'" -fps_mode passthrough \
        -c:v libx264 -preset veryfast -pix_fmt yuv420p "$out/h264_vfr.mkv"
    note h264_vfr.mkv

    # 4:4:4 H.264 (High 4:4:4): most hardware decoders refuse it, exercising the fallback.
    ff -f lavfi -i "$(video_src 30)" -c:v libx264 -preset veryfast -pix_fmt yuv444p \
        "$out/h264_444.mp4"
    note h264_444.mp4

    # Rotation through the display matrix (portrait phone video).
    ff -display_rotation 90 -i "$out/h264_30fps_aac.mp4" -c copy "$out/h264_rotated90.mp4"
    note h264_rotated90.mp4

    # Truncated file (invalid input for robustness tests).
    head -c 4096 "$out/h264_30fps_aac.mp4" >"$out/truncated.mp4"
    note truncated.mp4
else
    skip "H.264" "libx264 missing"
fi

# HEVC 10-bit (P010 in hardware decode).
if has_encoder libx265; then
    ff -f lavfi -i "$(video_src 30)" -c:v libx265 -preset ultrafast -pix_fmt yuv420p10le \
        -x265-params log-level=error "$out/hevc_10bit.mp4"
    note hevc_10bit.mp4
else
    skip "HEVC 10-bit" "libx265 missing"
fi

# AV1 (format of Omarchy screen recordings on recent GPUs).
if has_encoder libsvtav1; then
    ff -f lavfi -i "$(video_src 30)" -f lavfi -i "$(audio_src 48000)" \
        -c:v libsvtav1 -preset 12 -pix_fmt yuv420p -c:a libopus -shortest "$out/av1_opus.mkv"
    note av1_opus.mkv
elif has_encoder libaom-av1; then
    ff -f lavfi -i "$(video_src 30)" -c:v libaom-av1 -cpu-used 8 -pix_fmt yuv420p "$out/av1.mkv"
    note av1.mkv
else
    skip "AV1" "libsvtav1/libaom missing"
fi

# Standalone 44.1 kHz audio (sample rate conversion).
ff -f lavfi -i "$(audio_src 44100 2)" -c:a pcm_s16le "$out/tone_44100.wav"
note tone_44100.wav

# Steady hiss under a tone that starts after one second (noise reduction): seeded white noise
# at amplitude 0.02 for 3 s, plus the 440 Hz tone from 1 s on.
ff -f lavfi -i "anoisesrc=duration=3:color=white:amplitude=0.02:sample_rate=48000:seed=7" \
    -f lavfi -i "$(audio_src 48000 3)" \
    -filter_complex "[1]volume=volume=0:enable='lt(t,1)'[tone];[0][tone]amix=inputs=2:normalize=0" \
    -c:a pcm_s16le "$out/noisy_tone.wav"
note noisy_tone.wav

# Still image.
ff -f lavfi -i "testsrc2=size=640x360:rate=1:duration=1" -frames:v 1 "$out/still.png"
note still.png
# Flat color patches for color adjustment tests, 80 px each: dark neutral grey, mid neutral grey,
# a vivid orange and white.
ff -f lavfi -i "color=c=0x303030:size=80x180,format=yuv420p" \
    -f lavfi -i "color=c=0x777777:size=80x180,format=yuv420p" \
    -f lavfi -i "color=c=0xc05020:size=80x180,format=yuv420p" \
    -f lavfi -i "color=c=0xffffff:size=80x180,format=yuv420p" \
    -filter_complex "[0][1][2][3]hstack=inputs=4,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=tv" \
    -frames:v 1 -f yuv4mpegpipe "$out/color_patches.y4m"
note color_patches.y4m
# The same patches with color tags the decoder reports (Y4M carries none): lossless FFV1 in
# Matroska, BT.709 matrix at limited and at full range. At 180 lines an untagged frame falls
# back to BT.601, so a decoder that ignored the tags would shift the orange patch.
for range in tv pc; do
    ff -i "$out/color_patches.y4m" \
        -vf "scale=in_range=tv:out_range=$range:out_color_matrix=bt709,format=yuv420p,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=$range" \
        -c:v ffv1 -color_range "$range" \
        "$out/color_patches_bt709_$range.mkv"
    note "color_patches_bt709_$range.mkv"
done

# 2 s of tone whose packets from 1.0 to 1.5 s are missing but keep their timestamps (a capture
# that dropped audio): decoding must hold the later sound at 1.5 s, with silence in the gap.
ff -f lavfi -i "$(audio_src 48000 2)" -af "asetnsamples=n=480,aselect='not(between(t\,1\,1.499))'" \
    -c:a pcm_s16le "$out/tone_gap.mkv"
note tone_gap.mkv

# Two one-sample clicks (at samples 24000 and 60000 of 2 s, 48 kHz) in a file whose timestamps
# start three hours in: sample-exact placement at long source offsets (M10 audio boundaries).
ff -f lavfi -i "aevalsrc='if(eq(n\,24000)+eq(n\,60000)\,0.9\,0)':s=48000:d=2" \
    -c:a pcm_s16le -output_ts_offset 10800 "$out/pulse_3h.mkv"
note pulse_3h.mkv

echo "Fixtures in $out:"
printf '  %s\n' "${made[@]}"
if ((${#skipped[@]})); then
    echo "Skipped:"
    printf '  %s\n' "${skipped[@]}"
fi
