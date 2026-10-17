#!/usr/bin/env bash
set -euo pipefail

guard=$(realpath "${1:?release guard script required}")
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cd "$scratch"
git init -q -b master
git -c user.name=Test -c user.email=test@example.invalid commit -q --allow-empty -m base
git switch -q -c feature
git -c user.name=Test -c user.email=test@example.invalid commit -q --allow-empty -m feature
git tag v0.1.0-rc.1

# An unmerged tag must stop the release before its build or publish jobs.
if bash "$guard" v0.1.0-rc.1 master >/dev/null 2>&1; then
    echo 'unmerged release tag was accepted' >&2
    exit 1
fi

git switch -q master
bash "$guard" HEAD master
git merge -q --ff-only feature
bash "$guard" v0.1.0-rc.1 master
