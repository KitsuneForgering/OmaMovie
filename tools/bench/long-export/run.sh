#!/usr/bin/env bash
# M7 long-output check: a MINUTES-long 1080p30 edit (five-minute cuts of a ten-minute source,
# reordered, one dissolve, two sepia clips) exported by the app and checked by FFmpeg alone:
# frame count, A/V length, sound at the start, middle and end, and sampled frames (check.py).
# Usage: tools/bench/long-export/run.sh [minutes=120] [report-dir]   (opens the app's window)
# The output is large (a two-hour VA-API export is ~10 GB): it goes under the report dir.
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
minutes=${1:-120} # five-minute cuts; at least 25
dir="${2:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-bench/long-export}/$(date +%F-%H%M)"
src="${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures/long_1080p30_bt709_hue.mp4"
app="$repo/build/release/omamovie"
cli="$repo/build/release/tools/oma-project/oma-project"
mkdir -p "$dir" "$(dirname "$src")"
report="$dir/report.txt"

[[ -s $src ]] || ffmpeg -v error -y -f lavfi -i "testsrc2=s=1920x1080:r=30:d=600" \
    -f lavfi -i "sine=f=440:d=600" -vf "hue=h=360*t/600,scale=out_color_matrix=bt709:out_range=tv,format=yuv420p" \
    -c:v libx264 -preset veryfast -crf 23 -color_primaries bt709 -color_trc bt709 \
    -colorspace bt709 -color_range tv -c:a aac -shortest -movflags +faststart "$src" || exit 1

# A VA-API export takes ~85 MB a minute: refuse up front rather than fill the disk mid-way.
free_mb=$(df -Pm "$dir" | awk 'NR == 2 {print $4}')
((free_mb > minutes * 100)) || { echo "need $((minutes * 100)) MB free in $dir, have $free_mb" >&2; exit 1; }

python3 -I "$here/project.py" otio "$minutes" "$src" "$dir/edit.otio" &&
    "$cli" convert "$dir/edit.otio" "$dir/converted.omamovie" > "$dir/convert.txt" &&
    python3 -I "$here/project.py" looks "$minutes" "$dir/converted.omamovie" "$dir/edit.omamovie" &&
    python3 -I "$here/project.py" plan "$minutes" "$src" "$dir/plan.json" || exit 1

{
    echo "Long export $(date '+%F %T %z'): $minutes min, commit $(git -C "$repo" rev-parse --short HEAD)"
    echo "GPU: $(vulkaninfo --summary 2>/dev/null | grep -m1 deviceName | cut -d= -f2- | sed 's/^ //')"
    echo "load: $(cut -d' ' -f1-3 /proc/loadavg); import: $(tr '\n' ' ' < "$dir/convert.txt")"
    python3 -I "$here/../cross-editor/measure.py" timeout $((minutes * 60)) "$app" --export-audit \
        "$dir/edit.omamovie" "$dir/out.mp4" 2>&1 | grep -E '^(export audit|  |measure:)'
    [[ -s $dir/out.mp4 ]] || { echo "export failed: nothing to check"; exit 1; }
    want=$((minutes * 60 * 30))
    got=$(ffprobe -v error -select_streams v:0 -count_packets -show_entries stream=nb_read_packets \
        -of csv=p=0 "$dir/out.mp4")
    v=$(ffprobe -v error -select_streams v:0 -show_entries stream=duration -of csv=p=0 "$dir/out.mp4")
    a=$(ffprobe -v error -select_streams a:0 -show_entries stream=duration -of csv=p=0 "$dir/out.mp4")
    echo "frames $got/$want, video $v s, audio $a s, $(stat -c %s "$dir/out.mp4") bytes"
    for at in 1 $((minutes * 30)) $((minutes * 60 - 2)); do
        printf 'sound at %s s: ' "$at"
        ffmpeg -v info -ss "$at" -t 1 -i "$dir/out.mp4" -vn -af volumedetect -f null - 2>&1 |
            grep -o 'mean_volume: [-0-9.]* dB'
    done
    python3 -I "$here/check.py" "$dir/out.mp4" "$src" "$dir/plan.json"
    echo "load at end: $(cut -d' ' -f1-3 /proc/loadavg)"
} > "$report" 2>&1
rm -f "$dir/out.mp4" "$dir/converted.omamovie"
echo "$report"
