#!/usr/bin/env bash
# Repeated, seeded Session stress tests. Logs and a summary stay in the output directory.
# Usage: tools/bench/stress/run.sh [ROUNDS] [OUTPUT_DIR]
# OMA_STRESS_TRANSPORT_OPS (default 200, max 2000) and OMA_STRESS_SWITCH_OPS
# (default 40, max 200) increase work within each round.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../../.." && pwd)
rounds=${1:-3}
if [[ ! $rounds =~ ^[1-9][0-9]*$ ]] || ((rounds > 100)); then
    echo "ROUNDS must be 1..100" >&2
    exit 2
fi

out=${2:-$(mktemp -d /tmp/omamovie-stress.XXXXXXXX)}
mkdir -p "$out"
out=$(cd "$out" && pwd)
build=${BUILD:-debug}
if [[ ! $build =~ ^(debug|release|asan|tsan)$ ]]; then
    echo "BUILD must be debug, release, asan or tsan" >&2
    exit 2
fi
binary="$repo/build/$build/tests/app_tests"
fixture="$repo/tests/fixtures/generated/h264_30fps_aac.mp4"
if [[ ! -x $binary || ! -s $fixture ]]; then
    echo "Build app_tests and generate fixtures first: make fixtures; cmake --build build/$build --target app_tests -j2" >&2
    exit 2
fi

cd "$repo"
summary="$out/summary.txt"
{
    echo "OmaMovie Session stress $(date '+%F %T %z')"
    echo "commit $(git rev-parse --short HEAD)$(git diff --quiet || echo ' (dirty)')"
    bash "$repo/tools/bench/host-power.sh"
    echo "host $(uname -sr), load $(cut -d' ' -f1-3 /proc/loadavg)"
    echo "build=$build rounds=$rounds transport_ops=${OMA_STRESS_TRANSPORT_OPS:-200} switch_ops=${OMA_STRESS_SWITCH_OPS:-40}"
    echo "fixture $(sha256sum "$fixture")"
} > "$summary"

run_case() {
    local round=$1 seed=$2 label=$3 filter=$4 logfile
    logfile="$out/round-${round}-${label}.log"
    echo "round=$round seed=$seed case=$label" | tee -a "$summary"
    if timeout 600 env OMA_STRESS_SEED="$seed" \
        python3 -I "$repo/tools/bench/cross-editor/measure.py" "$binary" "$filter" \
        > "$logfile" 2>&1; then
        tail -n 3 "$logfile" | tee -a "$summary"
    else
        local code=$?
        echo "FAIL rc=$code log=$logfile" | tee -a "$summary"
        return "$code"
    fi
}

for ((round = 1; round <= rounds; ++round)); do
    seed=$((2026 + round * 7919))
    run_case "$round" "$seed" intents 'survives seeded'
    run_case "$round" "$seed" cancel 'cancels an export cleanly'
    run_case "$round" "$seed" shutdown 'shuts down while playing and exporting'
    run_case "$round" "$seed" resources 'keeps file descriptors and memory bounded'
done
echo "PASS logs=$out" | tee -a "$summary"
