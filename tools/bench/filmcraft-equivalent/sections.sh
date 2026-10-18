# Report sections driving OmaMovie's own audits. Sourced by run.sh ($app, $repo set there).
# hw = default VA-API path, sw = OMA_PREVIEW_HARDWARE=0; full resolution unless noted.

audit() { env "$@" 2>&1 | grep -E '^(M4|M6|export audit|  )' | grep -vE '^  (trim drag|TMP)'; }

section_playback() {
    echo; echo "== playback (--m4-audit, 8 s)"
    local f
    for f in a1080.mp4 a2160.mp4 hevc2160.mp4 vp92160.webm av12160.mp4; do
        echo "-- $f hw"; audit OMA_PREVIEW_SCALE=1 timeout 120 "$app" --m4-audit "$1/$f" 8 | grep -E 'audit(:| stages)'
        echo "-- $f sw"; audit OMA_PREVIEW_HARDWARE=0 OMA_PREVIEW_SCALE=1 timeout 120 "$app" --m4-audit "$1/$f" 8 |
            grep -E 'audit(:| stages)'
    done
}

section_seek_scrub() {
    echo; echo "== seek (playing) and scrub (paused), 24 seeks"
    local f hw
    for f in a1080.mp4 a2160.mp4 hevc2160.mp4; do
        for hw in 1 0; do
            echo "-- $f $([[ $hw == 1 ]] && echo hw || echo sw)"
            audit OMA_PREVIEW_HARDWARE=$hw OMA_PREVIEW_SCALE=1 timeout 180 "$app" --m4-seek-audit "$1/$f" 24
            audit OMA_PREVIEW_HARDWARE=$hw OMA_PREVIEW_SCALE=1 timeout 180 "$app" --m4-scrub-audit "$1/$f" 24
        done
    done
}

section_export() {
    echo; echo "== export (20 s 1080p H.264 + AAC)"
    local out
    out=$(mktemp -d)
    audit timeout 600 "$app" --export-audit "$1/a1080.mp4" "$out/a1080.mp4"
    rm -rf "$out"
}

section_timeline() {
    echo; echo "== timeline (--m6-timeline-audit)"
    audit timeout 600 "$app" --m6-timeline-audit 1000
    audit timeout 600 "$app" --m6-timeline-audit 5000
}

section_tests() {
    echo; echo "== headless tests (release)"
    make -j BUILD=release test 2>&1 | grep -E 'tests passed|Failed|\*\*\*' | head -20
}
