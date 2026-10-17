#!/usr/bin/env bash
# A release tag must name code that passed through the protected master branch.
set -euo pipefail

commit=${1:-HEAD}
protected_branch=${2:-origin/master}
if ! git merge-base --is-ancestor "$commit" "$protected_branch"; then
    echo 'Release tags must point to a commit already merged into master' >&2
    exit 1
fi
