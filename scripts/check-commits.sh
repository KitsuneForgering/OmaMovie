#!/usr/bin/env bash
# Checks that commit subjects follow Conventional Commits (in English, imperative).
# Usage: scripts/check-commits.sh <base>..<head>     (default: origin/master..HEAD)
#
# Accepted: <type>(<optional scope>)!?: <description>
# Types: feat fix docs style refactor perf test build ci chore revert

set -euo pipefail

range="${1:-origin/master..HEAD}"
pattern='^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([a-z0-9._/-]+\))?!?: [^ ].{0,98}[^.]$'

status=0
while IFS= read -r line; do
    sha="${line%% *}"
    subject="${line#* }"
    # Merge commits created by GitHub are exempt.
    if [[ $subject == Merge\ * ]]; then
        continue
    fi
    if [[ ! $subject =~ $pattern ]]; then
        echo "✕ ${sha:0:8} $subject"
        status=1
    fi
done < <(git log --format='%H %s' "$range")

if ((status)); then
    cat >&2 <<'EOF'

Commit subjects must follow Conventional Commits, for example:
  feat(timeline): add ripple delete
  fix(media): handle missing pts in vfr streams
  docs: describe the release process
Types: feat fix docs style refactor perf test build ci chore revert
Max 100 characters, no trailing period.
EOF
fi
exit $status
