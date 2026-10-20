#!/usr/bin/env bash
# Three-layer scene on both editors (protocol: Docs/implementation-plan-cross-editor-benchmarks.md):
# a full-frame background and two half-size layers in opposite quadrants, exported through
# hardware and software H.264 and compared with FFmpeg's own scale+overlay of the sources.
# Usage: tools/bench/cross-editor/layers.sh [repeats=3] [report-dir]   (opens OmaMovie's window)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
repeats=${1:-3}
out_dir="${2:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-bench/cross-editor}/$(date +%F-%H%M)-layers"
src="${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures/layers"
app="$repo/build/release/omamovie"
mkdir -p "$out_dir" "$src"
report="$out_dir/report.txt"
# shellcheck source=editors.sh
. "$here/editors.sh"

tag=(-vf "scale=out_color_matrix=bt709:out_range=tv,format=yuv420p,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=limited"
    -c:v libx264 -preset fast -crf 16 -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv)
i=0
for s in testsrc2 testsrc mandelbrot; do
    [[ -s $src/layer$i.mp4 ]] || ffmpeg -v error -y -f lavfi -i "$s=s=1920x1080:r=24000/1001" -t 10 "${tag[@]}" "$src/layer$i.mp4"
    i=$((i + 1))
done
[[ -s $src/reference.mp4 ]] || ffmpeg -v error -y -i "$src/layer0.mp4" -i "$src/layer1.mp4" -i "$src/layer2.mp4" \
    -filter_complex "[1:v]scale=960:540[b];[2:v]scale=960:540[c];[0:v][b]overlay=0:0[t];[t][c]overlay=960:540" \
    -c:v libx264 -crf 10 -preset fast "$src/reference.mp4"
python3 -I "$here/layers.py" "$repo/build/release/tools/oma-project/oma-project" "$src" "$out_dir/layers.omamovie"
melt_in=(-profile atsc_1080p_2398 "$src/layer0.mp4" in=0 out=239 -track "$src/layer1.mp4" in=0 out=239
    -track "$src/layer2.mp4" in=0 out=239 -transition affine a_track=0 b_track=1 "rect=0 0 960 540"
    -transition affine a_track=0 b_track=2 "rect=960 540 960 540")

{
    echo "Three-layer export $(date '+%F %T %z'), $repeats repeats, order alternated per round"
    echo "editors: $(editor_version oma); $(editor_version melt); $(ffmpeg -version | head -1)"
    bash "$repo/tools/bench/host-power.sh"
    echo "sources: $(cd "$src" && sha256sum layer*.mp4 | cut -c1-16 | tr '\n' ' ')"
    cases=("oma hardware" "melt hardware" "oma software" "melt software")
    for ((round = 1; round <= repeats; round++)); do
        order=("${cases[@]}")
        ((round % 2 == 0)) && order=("${cases[3]}" "${cases[2]}" "${cases[1]}" "${cases[0]}")
        for c in "${order[@]}"; do
            read -r editor route <<< "$c"
            out="$out_dir/$editor-$route-$round.mp4"
            if [[ $editor == oma ]]; then args=("$out_dir/layers.omamovie"); else args=("${melt_in[@]}"); fi
            m=$("export_$editor" "$route" "$out" "${args[@]}" 2>&1 | grep -E '^measure:' | cut -d' ' -f2-6)
            n=$(ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 "$out")
            p=$(ffmpeg -v info -i "$out" -i "$src/reference.mp4" -lavfi "[0:v][1:v]psnr" -f null - 2>&1 | grep -o 'average:[0-9.inf]*' | cut -d: -f2)
            echo "round $round $editor $route: $m frames=$n psnr_vs_reference=$p"
            [[ $n == 240 ]] && awk -v p="$p" 'BEGIN { exit !(p == "inf" || p >= 35) }' || { echo "FAIL: $c"; exit 1; }
            rm -f "$out"
        done
    done
    echo "load at end: $(cut -d' ' -f1-3 /proc/loadavg)"
} > "$report" 2>&1
echo "$report"
