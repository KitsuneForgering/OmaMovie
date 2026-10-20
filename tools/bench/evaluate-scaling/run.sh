#!/usr/bin/env bash
# Sweeps per-frame timeline::evaluate cost against the Release timeline library.
# Usage: tools/bench/evaluate-scaling/run.sh [output-dir]   (after `make BUILD=release libs`)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
out_dir=${1:-/tmp/omamovie-evaluate-scaling}
lib=$repo/build/release/lib
for a in "$lib/liboma_timeline.a" "$lib/liboma_base.a"; do
    [[ -s $a ]] || { echo "missing $a; run make BUILD=release libs" >&2; exit 1; }
done
mkdir -p "$out_dir"
bin=$out_dir/evaluate-scaling
"${CXX:-g++}" -std=c++23 -O2 -DNDEBUG -I"$repo/libs/timeline/include" \
    -I"$repo/libs/base/include" "$here/main.cpp" "$lib/liboma_timeline.a" \
    "$lib/liboma_base.a" -o "$bin"

echo "evaluate scaling $(date '+%F %T %z')"
echo "commit $(git -C "$repo" rev-parse HEAD)$(git -C "$repo" diff --quiet || echo ' (dirty)')"
echo "CPU: $(awk -F ': ' '/^model name/{print $2; exit}' /proc/cpuinfo)"
bash "$repo/tools/bench/host-power.sh"
for args in "1 1 100" "3 3 100" "3 2000 100" "8 50 100" "8 500 100" "16 500 100" \
    "32 500 50" "32 2000 50" "64 2000 25" "3 3 5000"; do
    # shellcheck disable=SC2086
    "$bin" $args
done
