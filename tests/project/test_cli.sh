#!/bin/sh
set -eu

cli=$1
file=$(mktemp)
trap 'rm -f "$file"' EXIT
printf '%s\n' '{"format":"omamovie-project","format_version":1}' > "$file"

"$cli" validate "$file" | grep -qx valid
"$cli" inspect "$file" | grep -qx 'clips: 0'
# dump writes the current format: a version 1 file comes out as the current version.
"$cli" dump "$file" | grep -q '"format_version": 6'
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
