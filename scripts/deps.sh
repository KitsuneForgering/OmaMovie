#!/usr/bin/env bash
# Lists or installs the project's dependencies, read from the PKGBUILD (single source of truth).
#
#   scripts/deps.sh                    print every dependency (one per line)
#   scripts/deps.sh --runtime          print only depends
#   scripts/deps.sh --install          install depends + makedepends + checkdepends + _devdepends
#   scripts/deps.sh --install --no-dev same, without _devdepends
#   scripts/deps.sh --install --noconfirm   non-interactive (CI)
#
# Installs with pacman (through sudo unless already root). Packages already installed are skipped.

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
pkgbuild="$root/PKGBUILD"

install=0
dev=1
runtime_only=0
noconfirm=()
for arg in "$@"; do
    case "$arg" in
    --install) install=1 ;;
    --no-dev) dev=0 ;;
    --runtime) runtime_only=1 ;;
    --noconfirm) noconfirm=(--noconfirm) ;;
    -h | --help)
        sed -n '2,10p' "$0"
        exit 0
        ;;
    *)
        echo "unknown option: $arg" >&2
        exit 2
        ;;
    esac
done

# Collect and normalize package arrays from the PKGBUILD.
source "$root/scripts/deps-packages.sh"
if ((!install)); then
    printf '%s\n' "${packages[@]}"
    exit 0
fi

sudo=()
if ((EUID != 0)); then
    sudo=(sudo)
fi
# base-devel is assumed by makepkg and provides make, gcc and pkgconf.
"${sudo[@]}" pacman -S --needed "${noconfirm[@]}" base-devel "${packages[@]}"
