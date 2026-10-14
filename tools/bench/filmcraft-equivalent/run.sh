#!/usr/bin/env bash
# FilmCraft-equivalent workload, the M1 spikes and the headless tests, in one dated report.
# Usage: tools/bench/filmcraft-equivalent/run.sh [report-dir]
#   report-dir defaults to $XDG_DATA_HOME/omamovie-bench, one folder per day (report.txt and the
#   spikes' full logs), older than 7 days removed; fixtures are cached in
#   $XDG_CACHE_HOME/omamovie-bench/fixtures (FilmCraft's recipes, never committed).
# The audits open the app's window for a few seconds each: run it in a graphical session.
# Results are comparable only on one machine; the report records machine, driver and load.
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
out_dir="${1:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-bench}"
fixtures="${XDG_CACHE_HOME:-$HOME/.cache}/omamovie-bench/fixtures"
run_dir="$out_dir/$(date +%F)"
mkdir -p "$run_dir" "$fixtures"
report="$run_dir/report.txt"
app="$repo/build/release/omamovie"
spikes="$repo/build/spikes/tools/spikes"
cd "$repo" || exit 1

# shellcheck source=fixtures.sh
. "$here/fixtures.sh"
# shellcheck source=decode.sh
. "$here/decode.sh"
# shellcheck source=sections.sh
. "$here/sections.sh"
# shellcheck source=spikes.sh
. "$here/spikes.sh"
# shellcheck source=report.sh
. "$here/report.sh"

{
    echo "OmaMovie bench $(date '+%F %T %z')"
    echo "commit $(git rev-parse --short HEAD) on $(git branch --show-current)$(git diff --quiet || echo ' (dirty)')"
    echo "CPU: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //'), $(nproc) threads"
    echo "GPU: $(vulkaninfo --summary 2>/dev/null | grep -m1 deviceName | cut -d= -f2- | sed 's/^ //')"
    echo "VA-API: $(vainfo 2>&1 | grep -m1 'Driver version' | cut -d: -f2- | sed 's/^ //')"
    echo "$(ffmpeg -version | head -1)"
    echo "power online: $(cat /sys/class/power_supply/A*/online 2>/dev/null | head -1), load at start: $(cut -d' ' -f1-3 /proc/loadavg)"
    echo
    echo "== build"
    make -j BUILD=release >/dev/null 2>&1 && echo "release: ok" || echo "release: FAILED"
    make spikes >/dev/null 2>&1 && echo "spikes: ok" || echo "spikes: FAILED"
    make_fixtures "$fixtures"
    section_decode "$fixtures"
    section_playback "$fixtures"
    section_seek_scrub "$fixtures"
    section_export "$fixtures"
    section_timeline
    section_spikes "$fixtures" "$run_dir/spikes"
    section_tests
    echo
    echo "load at end: $(cut -d' ' -f1-3 /proc/loadavg)"
} > "$report" 2>&1

compare_with_previous "$out_dir" "$report"
prune_old_runs "$out_dir"
echo "$report"
