#!/usr/bin/env bash
# Run one CI task in the same Arch Linux container used by the workflow.
set -euo pipefail

task=${1:?task required}
compiler=${2:-}
build=${3:-}
docker run --rm --cap-add SYS_PTRACE \
    -e OMA_CI_TASK="$task" -e OMA_CI_COMPILER="$compiler" -e OMA_CI_BUILD="$build" \
    -v "$PWD:/src" -w /src archlinux:latest bash -euo pipefail -c '
    pacman -Syu --noconfirm >/dev/null
    scripts/deps.sh --install --noconfirm >/dev/null
    case "$OMA_CI_TASK" in
        test)
            app=""
            case "$OMA_CI_BUILD" in debug|release) app="build/$OMA_CI_BUILD/omamovie" ;; esac
            make -j"$(nproc)" BUILD="$OMA_CI_BUILD" CXX="$OMA_CI_COMPILER" \
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
        lint) make format-check compdb tidy ;;
        fuzz) make fuzz FUZZ_RUNS=500000 ;;
        *) echo "unknown CI task: $OMA_CI_TASK" >&2; exit 2 ;;
    esac
    '
