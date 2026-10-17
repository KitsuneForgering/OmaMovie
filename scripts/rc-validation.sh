#!/usr/bin/env bash
# Pre-release correctness gate on the declared Intel/software machine. Requires a graphical
# session, FFmpeg, melt, enough free space for a two-hour export, and a release build.
# Usage: scripts/rc-validation.sh [report-dir] [long-minutes=120] [comparison-repeats=5]
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
report_dir=${1:-${XDG_DATA_HOME:-$HOME/.local/share}/omamovie-rc-validation/$(date +%F-%H%M)}
long_minutes=${2:-120}
repeats=${3:-5}
[[ $long_minutes =~ ^[0-9]+$ && $long_minutes -ge 25 ]] || { echo "long-minutes must be >=25" >&2; exit 2; }
[[ $repeats =~ ^[0-9]+$ && $repeats -ge 1 ]] || { echo "comparison-repeats must be >=1" >&2; exit 2; }
mkdir -p "$report_dir"
report="$report_dir/status.txt"
trap 'echo "FAIL: inspect $report and reports under $report_dir" >&2' ERR

{
    echo "RC validation $(date '+%F %T %z')"
    echo "commit $(git -C "$repo" rev-parse HEAD)"
    echo "working tree: $(git -C "$repo" status --short | wc -l) changed paths"
    echo "host $(uname -r); $(ffmpeg -version | head -1)"
    echo "== build and deterministic/sanitizer-independent tests"
    make -C "$repo" BUILD=release test
    echo "== extended stateful edit properties: 3000 seeds x 200 steps"
    OMA_PROPERTY_RUNS=3000 OMA_PROPERTY_STEPS=200 \
        "$repo/build/release/tests/timeline_tests"
    echo "== same-machine shared cuts, both encode routes, $repeats repeats"
    bash "$repo/tools/bench/cross-editor/run.sh" "$repeats" "$report_dir/cross-editor" cuts
    echo "== same-machine full source, both encode routes, $repeats repeats"
    bash "$repo/tools/bench/cross-editor/run.sh" "$repeats" "$report_dir/cross-editor" whole
    echo "== long-form edited output, $long_minutes minutes"
    bash "$repo/tools/bench/long-export/run.sh" "$long_minutes" "$report_dir/long-export"
    echo "PASS: all output invariants and test suites"
} > "$report" 2>&1
echo "$report"
