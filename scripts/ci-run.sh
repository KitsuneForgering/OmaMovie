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
        lint) make format-check compdb tidy ;;
        fuzz) make fuzz FUZZ_RUNS=500000 ;;
        *) echo "unknown CI task: $OMA_CI_TASK" >&2; exit 2 ;;
    esac
    '
