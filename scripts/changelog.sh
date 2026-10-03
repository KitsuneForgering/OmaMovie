#!/usr/bin/env bash
# Prints release notes in Markdown from Conventional Commits.
# Usage: scripts/changelog.sh <tag>   (notes for commits since the previous tag)

set -euo pipefail

tag="${1:?usage: scripts/changelog.sh <tag>}"
previous="$(git describe --tags --abbrev=0 "${tag}^" 2>/dev/null || true)"
range="${previous:+$previous..}$tag"

section() {
    local title="$1" types="$2" lines
    lines="$(git log --format='%s (%h)' "$range" | grep -E "^($types)(\([^)]*\))?!?: " || true)"
    if [[ -n $lines ]]; then
        printf '### %s\n\n' "$title"
        sed -E 's/^[a-z]+(\(([^)]*)\))?!?: /- **\2** /; s/- \*\*\*\* /- /' <<<"$lines"
        printf '\n'
    fi
}

breaking="$(git log --format='%s%n%b' "$range" | grep -E '^[a-z]+(\([^)]*\))?!: |^BREAKING CHANGE:' || true)"
if [[ -n $breaking ]]; then
    printf '### Breaking changes\n\n'
    sed 's/^/- /' <<<"$breaking"
    printf '\n'
fi

section "Features" "feat"
section "Fixes" "fix"
section "Performance" "perf"
section "Documentation" "docs"
section "Build, CI and tooling" "build|ci|chore"
section "Tests" "test"
section "Refactoring" "refactor|style"

if [[ -n $previous ]]; then
    printf '**Full diff:** `%s...%s`\n' "$previous" "$tag"
fi
