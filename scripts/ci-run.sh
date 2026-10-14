#!/usr/bin/env bash
# Run one CI task in the same Arch Linux container used by the workflows.
# Usage: scripts/ci-run.sh test <compiler> <build> | smoke | format | tidy | fuzz <target> | release
set -euo pipefail

task=${1:?task required}
arg1=${2:-}
arg2=${3:-}
# The release stage installs what the package needs (no development tools): it proves the
# PKGBUILD's depends/makedepends/checkdepends are enough to build and test what ships.
deps_flags=""
[[ $task == release ]] && deps_flags="--no-dev"
docker run --rm --cap-add SYS_PTRACE \
    -e OMA_CI_TASK="$task" -e OMA_CI_ARG1="$arg1" -e OMA_CI_ARG2="$arg2" -e OMA_CI_DEPS="$deps_flags" \
    -v "$PWD:/src" -w /src archlinux:latest bash -euo pipefail -c '
    pacman -Syu --noconfirm >/dev/null
    scripts/deps.sh --install $OMA_CI_DEPS --noconfirm >/dev/null
    case "$OMA_CI_TASK" in
        test) # compiler, build
            app=""
            case "$OMA_CI_ARG2" in debug|release) app="build/$OMA_CI_ARG2/omamovie" ;; esac
            make -j"$(nproc)" BUILD="$OMA_CI_ARG2" CXX="$OMA_CI_ARG1" \
                test $app JUNIT_DIR=build/junit
            ;;
        smoke)
            # The GUI smoke on a headless wlroots compositor with lavapipe (software Vulkan),
            # as scripts/package-check.sh runs it on the installed package. sway runs as a plain
            # user and keeps no file capabilities in the container.
            pacman -S --noconfirm --needed sway vulkan-swrast libcap >/dev/null
            make -j"$(nproc)" BUILD=release build/release/omamovie fixtures
            setcap -r /usr/bin/sway
            useradd -m smoke
            su smoke -c "
                export XDG_RUNTIME_DIR=/tmp/xdg WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman
                mkdir -m 700 \$XDG_RUNTIME_DIR && sway -c /dev/null >/tmp/sway.log 2>&1 &
                for i in \$(seq 50); do ls /tmp/xdg/wayland-? >/dev/null 2>&1 && break; sleep 0.2; done
                export WAYLAND_DISPLAY=\$(basename /tmp/xdg/wayland-?)
                QT_QPA_PLATFORM=wayland timeout 600 build/release/omamovie --smoke"
            ;;
        format) make format-check ;;
        tidy) make compdb tidy ;;
        fuzz) make "fuzz-${OMA_CI_ARG1:?fuzz target required}" FUZZ_RUNS=500000 ;; # target
        release) # the shipped tree, built and tested: stage/usr
            make -j"$(nproc)" BUILD=release test build/release/omamovie
            make BUILD=release DESTDIR=/src/stage PREFIX=/usr install
            ;;
        *) echo "unknown CI task: $OMA_CI_TASK" >&2; exit 2 ;;
    esac
    '
