#!/usr/bin/env bash
# M12 staged-export baseline. Run in a graphical Intel session after building omamovie.
# Usage: tools/bench/export-transfer/run.sh [report-dir] [repeats]
# Retains each movie and per-run log so frame/audio failures can be inspected afterward.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
out_dir=${1:-/tmp/omamovie-export-transfer-$(date +%F-%H%M%S)}
repeats=${2:-3}
[[ $repeats =~ ^[1-9][0-9]*$ ]] || { echo "repeats must be positive" >&2; exit 2; }
mkdir -p "$out_dir"
out_dir=$(cd "$out_dir" && pwd)
app=$repo/build/release/omamovie
measure=$repo/tools/bench/cross-editor/measure.py
cache=${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures
screen=$repo/tests/fixtures/generated/screenrecording_like.mp4
ultra=$cache/a2160.mp4
tone=$cache/a1080.mp4

for source in "$screen" "$ultra" "$tone"; do
    [[ -s $source ]] || { echo "missing fixture: $source" >&2; exit 1; }
done
[[ -x $app ]] || { echo "build/release/omamovie is missing" >&2; exit 1; }

report=$out_dir/report.txt
{
    echo "M12 staged export transfer baseline $(date '+%F %T %z')"
    echo "commit $(git -C "$repo" rev-parse HEAD)$(git -C "$repo" diff --quiet || echo ' (dirty)')"
    echo "CPU: $(awk -F ': ' '/^model name/{print $2; exit}' /proc/cpuinfo)"
    echo "GPU: $(vulkaninfo --summary 2>/dev/null | sed -n 's/.*deviceName[[:space:]]*= //p' | head -1)"
    echo "VA-API: $(vainfo 2>&1 | sed -n 's/.*Driver version: //p' | head -1)"
    ffmpeg -version | sed -n '1p'
    bash "$repo/tools/bench/host-power.sh"
    echo "load at start: $(cut -d' ' -f1-3 /proc/loadavg)"
    echo "repeats: $repeats; timeout: 120 seconds per export"
    sha256sum "$screen" "$ultra" "$tone"
    echo
} > "$report"

run_one() { # label source iteration
    local label=$1 source=$2 iteration=$3
    local movie=$out_dir/$label-$iteration.mp4
    local log=$out_dir/$label-$iteration.log psnr=$out_dir/$label-$iteration.psnr
    local expected actual
    echo "== $label run $iteration: $(date '+%F %T %z'); load $(cut -d' ' -f1-3 /proc/loadavg)" | tee -a "$report"
    python3 -I "$measure" timeout 120 "$app" --export-audit "$source" "$movie" > "$log" 2>&1
    rg '^(export audit:|  encoder:|  path:|  process CPU:|  per frame:|  decode output:|  transfers:|  compositor staging:|  NV12 readback phases:|  NV12 GPU conversion:|  writer:|  result:|measure:)' "$log" | tee -a "$report"
    ffprobe -v error -count_frames -select_streams v:0 \
        -show_entries stream=codec_name,width,height,r_frame_rate,nb_read_frames \
        -of default=noprint_wrappers=1 "$movie" | tee -a "$report"
    expected=$(ffprobe -v error -count_frames -select_streams v:0 \
        -show_entries stream=nb_read_frames -of default=noprint_wrappers=1:nokey=1 "$source")
    actual=$(ffprobe -v error -count_frames -select_streams v:0 \
        -show_entries stream=nb_read_frames -of default=noprint_wrappers=1:nokey=1 "$movie")
    [[ $actual == "$expected" ]] || { echo "frame count $actual != $expected" | tee -a "$report"; return 1; }
    ffmpeg -v error -i "$movie" -f null -
    ffmpeg -v error -i "$source" -i "$movie" \
        -lavfi "[0:v][1:v]psnr=stats_file=$psnr" -f null -
    awk -v expected="$expected" '{ split($6, x, ":"); v=x[2]; if (n==0 || v<min) min=v; sum+=v; n++ }
         END { if (n != expected) exit 1;
               printf "video PSNR: %d paired frames, min %.2f dB, mean %.2f dB\n", n, min, sum/n }' \
        "$psnr" | tee -a "$report"
}

for ((i=1; i<=repeats; ++i)); do
    run_one 1080p60 "$screen" "$i"
    run_one 4k23976 "$ultra" "$i"
done

# The timed 1080p60 and 4K fixtures are silent. Check an additional sine-wave export's
# decoded audio against its source instead of treating a silent track as audio fidelity.
run_one 1080p23976-tone "$tone" 1
ffmpeg -hide_banner -v info -i "$tone" -i "$out_dir/1080p23976-tone-1.mp4" \
    -filter_complex '[0:a]aresample=48000,atrim=duration=20,asetpts=PTS-STARTPTS[a];[1:a]atrim=duration=20,asetpts=PTS-STARTPTS[b];[a][b]apsnr[m]' \
    -map '[m]' -f null - > "$out_dir/audio-fidelity.log" 2>&1
rg 'PSNR ch' "$out_dir/audio-fidelity.log" | tee -a "$report"
echo "load at end: $(cut -d' ' -f1-3 /proc/loadavg)" | tee -a "$report"
echo "$report"
