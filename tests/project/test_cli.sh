#!/bin/sh
set -eu

cli=$1
file=$(mktemp)
trap 'rm -f "$file"' EXIT
printf '%s\n' '{"format":"omamovie-project","format_version":1}' > "$file"

"$cli" validate "$file" | grep -qx valid
"$cli" inspect "$file" | grep -qx 'clips: 0'
# dump writes the current format: a version 1 file comes out as the current version.
"$cli" dump "$file" | grep -q '"format_version": 7'
# An effect from a newer version is valid, kept, and reported (ADR-0016).
"$cli" validate tests/fuzz/corpus/project/effects.json 2>&1 >/dev/null |
    grep -q "unknown effect 'org.example.glow'"

printf '%s\n' '{' > "$file"
if "$cli" validate "$file" >/dev/null 2>&1; then
    exit 1
fi
if "$cli" unknown "$file" >/dev/null 2>&1; then
    exit 1
fi

# The OTIO import report (CLAUDE.md §16.4): missing media becomes reported gaps; a file next to
# the OTIO by relative path is found and its clip placed.
"$cli" import tests/importers/fixtures/basic.otio | grep -qx 'level reached: 2'
"$cli" import tests/importers/fixtures/basic.otio | grep -q 'media file not found'
"$cli" import tests/importers/fixtures/relative.otio | grep -qx 'media found: 1 of 1 (not probed)'
if "$cli" import tests/importers/fixtures/relative.otio | grep -q '^issue:'; then
    exit 1
fi

# convert: the same import with media probed, saved as a project that loads and validates; an
# existing output is refused.
converted="$file.converted.omamovie"
rm -f "$converted"
"$cli" convert tests/importers/fixtures/relative.otio "$converted" | grep -qx 'media: 1'
"$cli" validate "$converted" >/dev/null
"$cli" dump "$converted" | grep -q 'h264_30fps_aac.mp4'
# The canvas follows the first picture (the 320x180 fixture), not the 1920x1080 default.
"$cli" inspect "$converted" | grep -qx 'canvas: 320 x 180'
status=0
"$cli" convert tests/importers/fixtures/relative.otio "$converted" >/dev/null 2>&1 || status=$?
test "$status" -eq 2
rm -f "$converted"

# diff (CLAUDE.md §16.7): equal projects exit 0; different ones list their lines and exit 1.
other=tests/fuzz/corpus/project/effects.json
"$cli" diff "$other" "$other" >/dev/null
printf '%s\n' '{"format":"omamovie-project","format_version":1}' > "$file"
status=0
"$cli" diff "$file" "$other" > "$file.out" || status=$?
test "$status" -eq 1
grep -q "$(printf '^+[0-9]*\t')" "$file.out"
rm -f "$file.out"
# A one-field change names its field (reverse engineering by minimal pairs, CLAUDE.md §16.6).
"$cli" dump "$other" > "$file.a.json"
sed 's/"width": 1920/"width": 1280/' "$file.a.json" > "$file.b.json"
status=0
"$cli" diff "$file.a.json" "$file.b.json" > "$file.out" || status=$?
test "$status" -eq 1
test "$(cut -f2 "$file.out" | sort -u)" = "canvas.width"
rm -f "$file.out" "$file.a.json" "$file.b.json"
