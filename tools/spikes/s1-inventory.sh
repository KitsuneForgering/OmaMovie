#!/usr/bin/env bash
# Spike S1: inventory of the video decode/encode paths this machine actually supports.
# Usage: tools/spikes/s1-inventory.sh [output-dir]      (report in Docs/spikes/S1-hardware-inventory.md)
#
# A path counts as hardware only if FFmpeg reports the hardware pixel format as chosen by
# get_format(); FFmpeg silently falls back to software decoding otherwise, so "no error" is
# not evidence. Vulkan Video is probed with and without ANV_DEBUG=video-decode,video-encode,
# because Mesa's Intel driver keeps it behind that debug flag on some generations.

set -euo pipefail

out="${1:-$(mktemp -d)}"
mkdir -p "$out"
export SVT_LOG=1
anv_flag="ANV_DEBUG=video-decode,video-encode"

for tool in ffmpeg ffprobe vulkaninfo vainfo; do
    command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 1; }
done

ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }
src() { echo "testsrc2=size=1920x1080:rate=30:duration=${1:-1}"; }

echo "== Machine"
vulkaninfo --summary 2>/dev/null | { grep -E 'deviceName|driverName|driverInfo|apiVersion' || true; } | sed 's/^\s*/  /'
vainfo 2>&1 | { grep -E 'Driver version' || true; } | sed 's/^vainfo: /  /'
echo "  $(ffmpeg -version | head -1)"

echo
echo "== Vulkan Video extensions"
for env in "" "$anv_flag"; do
    exts=$(env $env vulkaninfo 2>/dev/null | { grep -oE 'VK_KHR_video_[a-z0-9_]+' || true; } | sort -u | tr '\n' ' ')
    printf '  %-36s %s\n' "${env:-(default)}" "${exts:-none}"
done

echo
echo "== Test media (1080p, 1 s)"
ff -f lavfi -i "$(src)" -c:v libx264 -preset veryfast -pix_fmt yuv420p "$out/h264.mp4"
ff -f lavfi -i "$(src)" -c:v libx265 -preset ultrafast -pix_fmt yuv420p10le -x265-params log-level=error "$out/hevc10.mp4"
ff -f lavfi -i "$(src)" -c:v libsvtav1 -preset 12 -pix_fmt yuv420p "$out/av1.mkv"
ff -f lavfi -i "$(src)" -c:v prores_ks -profile:v 2 -pix_fmt yuv422p10le "$out/prores.mov"
echo "  generated in $out"

# Prints the pixel format FFmpeg finally chose for decoding (hardware format = hardware path).
chosen_format() {
    local env="$1" hw="$2" file="$3"
    env $env ffmpeg -hide_banner -loglevel debug -hwaccel "$hw" -hwaccel_output_format "$hw" \
        -i "$file" -map 0:v -frames:v 10 -f null - 2>&1 |
        { grep -oE 'Format [a-z0-9_]+ chosen by get_format' || true; } | tail -1 | awk '{print $2}'
}

echo
echo "== Decode (pixel format chosen; a hardware format means a hardware path)"
printf '  %-12s %-10s %-10s %-16s %-10s\n' media vaapi qsv "vulkan" "vulkan+flag"
for file in h264.mp4 hevc10.mp4 av1.mkv prores.mov; do
    printf '  %-12s %-10s %-10s %-16s %-10s\n' "$file" \
        "$(chosen_format "" vaapi "$out/$file")" \
        "$(chosen_format "" qsv "$out/$file")" \
        "$(chosen_format "" vulkan "$out/$file")" \
        "$(chosen_format "$anv_flag" vulkan "$out/$file")"
done

encode() {
    local name="$1" output="$2"
    shift 2
    if "$@" "$out/$output" 2>/dev/null && [[ -s $out/$output ]]; then
        printf '  %-28s OK    %s\n' "$name" "$(ffprobe -v error -select_streams v:0 \
            -show_entries stream=codec_name,pix_fmt -of csv=p=0 "$out/$output")"
    else
        printf '  %-28s FAIL\n' "$name"
    fi
}

echo
echo "== Encode"
# Plain ffmpeg (not the ff function) so the arrays also work behind `env`.
ffq=(ffmpeg -hide_banner -loglevel error -y)
vaapi=("${ffq[@]}" -vaapi_device /dev/dri/renderD128 -f lavfi -i "$(src)" -vf format=nv12,hwupload)
vk=("${ffq[@]}" -init_hw_device vulkan -f lavfi -i "$(src)")
encode h264_vaapi e_h264_vaapi.mp4 "${vaapi[@]}" -c:v h264_vaapi
encode hevc_vaapi e_hevc_vaapi.mp4 "${vaapi[@]}" -c:v hevc_vaapi
encode av1_vaapi e_av1_vaapi.mp4 "${vaapi[@]}" -c:v av1_vaapi
encode h264_qsv e_h264_qsv.mp4 ff -f lavfi -i "$(src)" -c:v h264_qsv
encode hevc_qsv e_hevc_qsv.mp4 ff -f lavfi -i "$(src)" -c:v hevc_qsv
encode "h264_vulkan" e_h264_vk.mp4 "${vk[@]}" -vf format=nv12,hwupload -c:v h264_vulkan
encode "h264_vulkan (+flag)" e_h264_vkf.mp4 env $anv_flag "${vk[@]}" -vf format=nv12,hwupload -c:v h264_vulkan
encode "hevc_vulkan (+flag)" e_hevc_vkf.mp4 env $anv_flag "${vk[@]}" -vf format=nv12,hwupload -c:v hevc_vulkan
encode "ffv1_vulkan yuv444p" e_ffv1.mkv "${vk[@]}" -vf format=yuv444p,hwupload -c:v ffv1_vulkan
encode "prores_ks_vulkan 422 10-bit" e_prores.mov "${vk[@]}" -vf format=yuv422p10le,hwupload -c:v prores_ks_vulkan

echo
echo "== Compute-codec decode (no flag)"
printf '  %-12s %s\n' "ffv1" "$(chosen_format "" vulkan "$out/e_ffv1.mkv")"
printf '  %-12s %s\n' "prores" "$(chosen_format "" vulkan "$out/e_prores.mov")"
