# The M1 hardware spikes (Docs/spikes/), rerun to see whether drivers or code moved them.
# Sourced by run.sh ($repo, $spikes set there). Raw output is kept; each spike prints its own
# summary lines.

section_spikes() {
    local fx=$1 gen="$repo/tests/fixtures/generated" work
    work=$(mktemp -d)
    echo; echo "== S1 hardware inventory"
    timeout 600 "$repo/tools/spikes/s1-inventory.sh" "$work/s1" 2>&1 | tail -40
    echo; echo "== S2 FFmpeg on OmaMovie's device"
    timeout 120 "$spikes/s2_ffmpeg_own_device" "$fx/a1080.mp4" --frames 300 2>&1 | tail -4
    echo; echo "== S3 decode paths (RUNS=3)"
    RUNS=3 timeout 1800 "$repo/tools/spikes/s3-decode-paths.sh" "$work/s3" 2>&1 | grep -vE '^\s*$' | tail -40
    echo; echo "== S4 Qt shared device (offscreen)"
    timeout 120 "$spikes/s4_qt_shared_device" 2>&1 | grep -v '^qt\.' | tail -3
    echo; echo "== S5 minimal compositing (300 frames)"
    local mode
    for mode in hardware software; do
        echo "-- $mode"
        timeout 300 "$spikes/s5_minimal_compositing" "$fx/a1080.mp4" "$gen/h264_30fps_aac.mp4" "$gen/still.png" \
            "$mode" 300 2>&1 | tail -3
    done
    echo; echo "== S6 libplacebo"
    timeout 300 "$spikes/s6_libplacebo" "$fx/a1080.mp4" 2>&1 | tail -4
    rm -rf "$work"
}
