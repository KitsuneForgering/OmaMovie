#!/usr/bin/env bash
# Builds the PKGBUILD from this checkout in a clean Arch container (makepkg runs check(), the
# whole test suite), installs the package in a second clean container and runs the GUI smoke
# under a headless wlroots compositor (sway) with lavapipe. Needs podman; about 10 minutes.
# Usage: scripts/package-check.sh [output-directory]   (packages and logs are written there)
set -euo pipefail
repo="$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)"
branch="$(git -C "$repo" rev-parse --abbrev-ref HEAD)"
out="$(realpath -m "${1:-$repo/build/package-check}")"
mkdir -p "$out"
image=docker.io/library/archlinux:latest

# The PKGBUILD clones the published repository; point it at this checkout instead.
podman run --rm -v "$repo:/src:ro,Z" -v "$out:/out:Z" "$image" bash -euo pipefail -c "
  pacman -Syu --noconfirm --needed base-devel git sudo >/dev/null
  useradd -m builder && echo 'builder ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/builder
  install -d -o builder /home/builder/pkg && cd /home/builder/pkg
  sed 's|^source=.*|source=(\"\${pkgname}::git+file:///src#branch=$branch\")|' /src/PKGBUILD > PKGBUILD
  chown builder PKGBUILD # makepkg rewrites pkgver in it
  sudo -u builder git config --global --add safe.directory '*'
  sudo -u builder makepkg -s --noconfirm > /out/makepkg.log 2>&1
  cp ./*.pkg.tar.zst /out/"

# sway carries file capabilities that rootless podman cannot grant; drop them in the throwaway
# container. The smoke reads tests/fixtures/generated relative to its working directory.
podman run --rm -v "$repo:/src:ro,Z" -v "$out:/out:Z" "$image" bash -uo pipefail -c '
  pacman -Syu --noconfirm --needed sway vulkan-swrast libcap >/dev/null
  pacman -U --noconfirm /out/omamovie-git-r*-x86_64.pkg.tar.zst >/dev/null
  setcap -r /usr/bin/sway; useradd -m user
  su user -c "
    export XDG_RUNTIME_DIR=/tmp/xdg WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman
    mkdir -m 700 \$XDG_RUNTIME_DIR && sway -c /dev/null >/tmp/sway.log 2>&1 &
    for i in \$(seq 50); do ls /tmp/xdg/wayland-? >/dev/null 2>&1 && break; sleep 0.2; done
    export XDG_RUNTIME_DIR=/tmp/xdg WAYLAND_DISPLAY=\$(basename /tmp/xdg/wayland-?)
    cd /src && QT_QPA_PLATFORM=wayland timeout 300 omamovie --smoke >/tmp/smoke.log 2>&1
    echo \"smoke exit \$?\" >>/tmp/smoke.log"
  cp /tmp/smoke.log /out/smoke.log'
grep -E "GUI smoke: PASS|smoke exit" "$out/smoke.log"
grep -q "smoke exit 0" "$out/smoke.log"
