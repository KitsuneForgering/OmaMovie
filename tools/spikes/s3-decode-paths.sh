#!/usr/bin/env bash
# Spike S3: decode throughput and CPU cost per path (software, VA-API, QSV, flagged Vulkan
# Video) and FFv1 vs ProRes as proxy intermediates.
# Usage: tools/spikes/s3-decode-paths.sh [output-dir]   (report in Docs/spikes/S3-decode-paths.md)
#
# Decoding keeps hardware frames on the GPU (-hwaccel_output_format) and discards them, so
# the numbers are decoder throughput only: no mapping into OmaMovie's device, no compositing,
# no presentation. Each run reports FFmpeg's -benchmark rtime/utime/stime; the median of
# RUNS is printed. A path is reported only when FFmpeg negotiated the hardware format (S1:
# FFmpeg falls back to software silently). Package power comes from RAPL when readable
# (usually root only); otherwise it is reported as n/a.

set -euo pipefail

out="${1:-$(mktemp -d)}"
runs="${RUNS:-5}"
secs="${SECS:-10}"
mkdir -p "$out"
export SVT_LOG=1
anv_flag="ANV_DEBUG=video-decode,video-encode"
rapl=/sys/class/powercap/intel-rapl:0/energy_uj

ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }
src="testsrc2=size=1920x1080:rate=60:duration=$secs"
frames=$((secs * 60))

echo "== Machine"
vulkaninfo --summary 2>/dev/null | { grep -E 'deviceName|driverInfo' || true; } | sed 's/^\s*/  /'
vainfo 2>&1 | { grep -E 'Driver version' || true; } | sed 's/^vainfo: /  /'
echo "  $(ffmpeg -version | head -1)"
echo "  CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //'), $(nproc) threads"
echo "  runs=$runs, ${secs}s 1080p60 ($frames frames)"

echo
echo "== Test media"
ff -f lavfi -i "$src" -c:v libx264 -preset veryfast -g 120 -pix_fmt yuv420p "$out/h264.mp4"
ff -f lavfi -i "$src" -c:v libx265 -preset ultrafast -g 120 -pix_fmt yuv420p10le -x265-params log-level=error "$out/hevc10.mp4"
ff -f lavfi -i "$src" -c:v libsvtav1 -preset 12 -g 120 -pix_fmt yuv420p "$out/av1.mkv"
echo "  generated in $out"

# Hardware format FFmpeg chose for the file, or empty (S1 method).
chosen_format() {
    local env="$1" hw="$2" file="$3"
    (env $env ffmpeg -hide_banner -loglevel debug -hwaccel "$hw" -hwaccel_output_format "$hw" \
        -i "$file" -map 0:v -frames:v 5 -f null - 2>&1 || true) |
        { grep -oE 'Format [a-z0-9_]+ chosen by get_format' || true; } | tail -1 | awk '{print $2}'
}

# One timed run: prints "rtime cpu_seconds joules" (joules = n/a without RAPL).
timed() {
    local e0="" e1 log
    [[ -r $rapl ]] && e0=$(<"$rapl")
    log=$("$@" -benchmark -f null - 2>&1 | grep -oE 'bench: utime=[0-9.]+s stime=[0-9.]+s rtime=[0-9.]+s' | tail -1)
    local u s r
    u=$(sed -E 's/.*utime=([0-9.]+)s.*/\1/' <<<"$log")
    s=$(sed -E 's/.*stime=([0-9.]+)s.*/\1/' <<<"$log")
    r=$(sed -E 's/.*rtime=([0-9.]+)s.*/\1/' <<<"$log")
    if [[ -n $e0 ]]; then e1=$(<"$rapl"); awk -v r="$r" -v u="$u" -v s="$s" -v a="$e0" -v b="$e1" 'BEGIN { print r, u + s, (b - a) / 1e6 }'
    else awk -v r="$r" -v u="$u" -v s="$s" 'BEGIN { print r, u + s, "n/a" }'; fi
}

# Median over RUNS of each column, printed as fps, CPU % of one core, joules.
measure() {
    local i results=()
    for ((i = 0; i < runs; i++)); do results+=("$(timed "$@")"); done
    printf '%s\n' "${results[@]}" | sort -n -k1 | awk -v n="$runs" -v f="$frames" '
        { r[NR] = $1; c[NR] = $2; j[NR] = $3 }
        END { m = int((n + 1) / 2)
              printf "%8.0f fps %6.0f%% CPU %8s J\n", f / r[m], 100 * c[m] / r[m],
                     (j[m] == "n/a" ? "n/a" : sprintf("%.1f", j[m])) }'
}

decode() {
    local label="$1" env="$2" hw="$3" file="$4"
    if [[ -n $hw ]]; then
        local fmt
        fmt=$(chosen_format "$env" "$hw" "$file")
        if [[ $fmt != "$hw" ]]; then printf '  %-10s %-14s fallback (%s)\n' "$(basename "$file")" "$label" "${fmt:-none}"; return; fi
        printf '  %-10s %-14s %s\n' "$(basename "$file")" "$label" \
            "$(measure env $env ffmpeg -hide_banner -nostats -hwaccel "$hw" -hwaccel_output_format "$hw" -i "$file" -map 0:v)"
    else
        printf '  %-10s %-14s %s\n' "$(basename "$file")" "$label" \
            "$(measure ffmpeg -hide_banner -nostats -i "$file" -map 0:v)"
    fi
}

echo
echo "== Decode throughput (median of $runs; frames discarded on the GPU)"
for file in h264.mp4 hevc10.mp4 av1.mkv; do
    decode software "" "" "$out/$file"
    decode vaapi "" vaapi "$out/$file"
    decode qsv "" qsv "$out/$file"
    decode "vulkan+flag" "$anv_flag" vulkan "$out/$file"
done

echo
echo "== Proxy intermediates (from h264.mp4: ProRes LT 4:2:2 10-bit, FFv1 4:2:0 8-bit level 4)"
# prores_ks_vulkan (FFmpeg 9.0.1, ANV) writes 1080p streams that no decoder accepts ("slice out
# of bounds"; 320x240 decodes), so the ProRes candidate is encoded on the CPU and only decoded
# on the GPU. Its encode check runs below so a fixed FFmpeg shows up here.
vk=(ffmpeg -hide_banner -loglevel error -y -init_hw_device vulkan -i "$out/h264.mp4")
"${vk[@]}" -t 1 -vf format=yuv422p10le,hwupload -c:v prores_ks_vulkan "$out/prores_vk.mov"
if ffmpeg -hide_banner -loglevel error -xerror -i "$out/prores_vk.mov" -f null - 2>/dev/null; then
    echo "  prores_ks_vulkan 1080p output decodes"
else
    echo "  prores_ks_vulkan 1080p output does NOT decode"
fi
ff -i "$out/h264.mp4" -c:v prores_ks -profile:v 1 -pix_fmt yuv422p10le "$out/proxy_prores.mov"
"${vk[@]}" -vf format=yuv420p,hwupload -c:v ffv1_vulkan -level 4 -strict experimental -g 60 "$out/proxy_ffv1.mkv"
for proxy in proxy_prores.mov proxy_ffv1.mkv; do
    size=$(stat -c %s "$out/$proxy")
    psnr=$(ffmpeg -hide_banner -i "$out/$proxy" -i "$out/h264.mp4" \
        -lavfi "[0:v]format=yuv420p,settb=1/60,setpts=N[a];[1:v]format=yuv420p,settb=1/60,setpts=N[b];[a][b]psnr" -f null - 2>&1 |
        grep -oE 'average:[0-9.inf]+' | tail -1 | cut -d: -f2)
    printf '  %-16s %6.1f Mbit/s, PSNR %s dB vs source\n' "$proxy" "$(awk -v b="$size" -v s="$secs" 'BEGIN { print b * 8 / s / 1e6 }')" "$psnr"
    decode software "" "" "$out/$proxy"
    decode vulkan "" vulkan "$out/$proxy"
done

# Seek cost: process start to one decoded frame, at the start and after seeking to the middle
# (the difference is the seek plus decoding from the previous keyframe; GOP = 2 s, FFv1 1 s).
echo
echo "== Open + first frame vs. seek to the middle + one frame (median of $runs, ms, software)"
one_frame_ms() {
    local i t0 results=()
    for ((i = 0; i < runs; i++)); do
        t0=$(date +%s%N)
        ffmpeg -hide_banner -loglevel error "$@" -frames:v 1 -f null -
        results+=($(( ($(date +%s%N) - t0) / 1000000 )))
    done
    printf '%s\n' "${results[@]}" | sort -n | awk -v n="$runs" '{ v[NR] = $1 } END { print v[int((n + 1) / 2)] }'
}
for file in h264.mp4 hevc10.mp4 av1.mkv proxy_prores.mov proxy_ffv1.mkv; do
    printf '  %-16s start %4s ms   middle %4s ms\n' "$file" \
        "$(one_frame_ms -i "$out/$file")" "$(one_frame_ms -ss $((secs / 2)).5 -i "$out/$file")"
done
