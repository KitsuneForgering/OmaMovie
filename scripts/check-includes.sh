#!/usr/bin/env bash
# The include rules of CLAUDE.md §5.2 that the build cannot see on its own: FFmpeg and Vulkan
# headers live in /usr/include, so any library could include them and still compile.
#   - FFmpeg (libav*, libsw*) only inside libs/media (tools/ may use it for diagnostics);
#   - no Qt headers in libs/;
#   - no Vulkan, CUDA or FFmpeg in the public headers of timeline, project, project-ir,
#     importers or base.
# Exit 1 and list the offending lines when a rule is broken.
set -euo pipefail
cd "$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)"
status=0
report() { # rule, matches
    if [ -n "$2" ]; then
        printf '%s:\n%s\n' "$1" "$2"
        status=1
    fi
}
report "FFmpeg outside libs/media" \
    "$(grep -rnE '#include <lib(av|sw)[a-z]*/' libs apps --include='*.cpp' --include='*.hpp' |
        grep -v '^libs/media/' || true)"
report "Qt in libs" \
    "$(grep -rnE '#include <Q[A-Za-z]' libs --include='*.cpp' --include='*.hpp' || true)"
report "Vulkan, CUDA or FFmpeg in model public headers" \
    "$(grep -rnE '#include <(vulkan|cuda|lib(av|sw))' \
        libs/{timeline,project,project-ir,importers,base}/include || true)"
exit "$status"
