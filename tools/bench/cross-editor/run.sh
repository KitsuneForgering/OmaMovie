#!/usr/bin/env bash
# Same-machine export comparison of OmaMovie and MLT's melt (Kdenlive's engine): one scene of a
# BT.709 source through hardware and software H.264, every output checked by FFmpeg (protocol:
# Docs/implementation-plan-cross-editor-benchmarks.md). Opens OmaMovie's window; run it idle.
# Usage: tools/bench/cross-editor/run.sh [repeats=5] [report-dir] [scene=whole|cuts] (scenes.sh)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
repeats=${1:-5}
scene=${3:-whole}
out_dir="${2:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-bench/cross-editor}/$(date +%F-%H%M)-$scene"
fixtures="${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures"
app="$repo/build/release/omamovie"
cli="$repo/build/release/tools/oma-project/oma-project"
src="$fixtures/xed_1080p24_bt709_noise-v3.mp4"
mkdir -p "$out_dir" "$fixtures"
report="$out_dir/report.txt"
trap 'echo "FAIL: $report" >&2' ERR

# shellcheck source=editors.sh
. "$here/editors.sh"
# shellcheck source=verify.sh
. "$here/verify.sh"
# shellcheck source=cuts.sh
. "$here/cuts.sh"
# shellcheck source=scenes.sh
. "$here/scenes.sh"

# 20 s 1080p 23.976 grain (FilmCraft's picture recipe) and seeded noise audio. The audio
# signature makes every source interval distinguishable after cuts and lossy re-encoding.
[[ -s $src ]] || ffmpeg -v error -y -f lavfi -i "testsrc2=s=1920x1080:r=24000/1001:d=20,noise=alls=6:allf=t" \
    -f lavfi -i "anoisesrc=c=pink:seed=739:a=0.2:d=20:r=48000" \
    -vf "scale=out_color_matrix=bt709:out_range=tv,format=yuv420p,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=limited" \
    -c:v libx264 -preset fast -crf 20 -color_primaries bt709 -color_trc bt709 -colorspace bt709 \
    -color_range tv -c:a aac -shortest -movflags +faststart "$src" || exit 1

{
    echo "Cross-editor export $(date '+%F %T %z'), $repeats repeats, order alternated per round"
    echo "CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //'), $(nproc) threads"
    echo "GPU: $(vulkaninfo --summary 2>/dev/null | grep -m1 deviceName | cut -d= -f2- | sed 's/^ //')"
    echo "VA-API: $(vainfo 2>&1 | grep -m1 'Driver version' | cut -d: -f2- | sed 's/^ //')"
    echo "$(ffmpeg -version | head -1)"
    echo "power online: $(cat /sys/class/power_supply/A*/online 2>/dev/null | head -1), load: $(cut -d' ' -f1-3 /proc/loadavg)"
    echo "editors: $(editor_version oma); $(editor_version melt)"
    scene_inputs "$scene" "$src" "$out_dir" || exit 1
    echo "scene: $scene; source: $(basename "$src") sha256 $(sha256sum "$src" | cut -c1-16), $(frames_of "$src") frames"
    cases=("oma hardware" "melt hardware" "oma software" "melt software")
    for ((round = 1; round <= repeats; round++)); do
        order=("${cases[@]}")
        ((round % 2 == 0)) && order=("${cases[3]}" "${cases[2]}" "${cases[1]}" "${cases[0]}")
        for c in "${order[@]}"; do # reversed every other round: neither editor always goes first
            read -r editor route <<< "$c"
            out="$out_dir/$editor-$route-$round.mp4"
            if [[ $editor == oma ]]; then args=("$oma_in"); else args=("${melt_in[@]}"); fi
            m=$("export_$editor" "$route" "$out" "${args[@]}" 2>&1 | grep -E '^measure:|^  encoder:' | tr '\n' ' ')
            m=${m% }
            echo "round $round $editor $route: $m"
            python3 -I "$here/assert_output.py" "$scene" "$src" "$out"
            rm -f "$out"
        done
    done
    echo "load at end: $(cut -d' ' -f1-3 /proc/loadavg)"
} > "$report" 2>&1
echo "$report"
