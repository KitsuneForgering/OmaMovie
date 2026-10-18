#!/usr/bin/env bash
# MLT round trip (Kdenlive's engine and OmaMovie on one project): melt writes an MLT document of
# two cuts with a luma mix, renders it itself, OmaMovie imports the same document (oma-project
# convert, then --export-audit) and the two movies are compared frame by frame by FFmpeg.
# Outside the mix they must match (both are lossy encodes of the same frames); inside it they
# differ by design: OmaMovie centres a dissolve on its cut and mixes in linear light (reported).
# Usage: tools/bench/cross-editor/roundtrip.sh [report-dir]
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
dir="${1:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-bench/cross-editor}/$(date +%F-%H%M)-roundtrip"
src="${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures/xed_1080p24_bt709.mp4"
app="$repo/build/release/omamovie"
cli="$repo/build/release/tools/oma-project/oma-project"
mkdir -p "$dir"
[[ -s $src ]] || { echo "run tools/bench/cross-editor/run.sh once: it makes $src" >&2; exit 1; }
# shellcheck source=editors.sh
. "$here/editors.sh"
# shellcheck source=verify.sh
. "$here/verify.sh"

mix=12 # frames; the cut between the two clips falls after A's first 60 frames
{
    echo "MLT round trip $(date '+%F %T %z'): $(editor_version oma); $(editor_version melt)"
    melt -quiet "$src" in=48 out=119 "$src" in=200 out=271 -mix "$mix" -mixer luma \
        -consumer "xml:$dir/mix.mlt" 2> /dev/null
    melt -quiet "$dir/mix.mlt" -consumer "avformat:$dir/melt.mp4" vcodec=libx264 crf=12 \
        preset=fast real_time=-1 2> /dev/null
    "$cli" convert "$dir/mix.mlt" "$dir/mix.omamovie" 2>&1 | grep -E '^(issue|level)'
    export_oma software "$dir/oma.mp4" "$dir/mix.omamovie" 2>&1 | grep -E '^(measure:|  result)'
    for f in melt oma; do
        echo "$f: $(frames_of "$dir/$f.mp4") frames"
    done
    ffmpeg -v error -i "$dir/oma.mp4" -i "$dir/melt.mp4" -lavfi "[0:v][1:v]psnr=stats_file=$dir/psnr.txt" \
        -f null -
    # Frames 1-60 and 80 on are outside both editors' transition windows.
    awk -v lo=60 -v hi=80 '{ split($1, n, ":"); split($6, p, ":");
        if (n[2] <= lo || n[2] >= hi) { out++; if (min == "" || p[2] < min) min = p[2] }
        else { inside++; if (low == "" || p[2] < low) low = p[2] } }
        END { printf "outside the mix: %d frames, worst %.1f dB; inside: %d frames, worst %.1f dB; %s\n",
              out, min, inside, low, min >= 35 ? "PASS" : "FAIL" }' "$dir/psnr.txt"
} > "$dir/report.txt" 2>&1
rm -f "$dir/melt.mp4" "$dir/oma.mp4"
echo "$dir/report.txt"
