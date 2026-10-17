# Release tag and installed app gates — 2026-10-17

**Classification:** RELEASE GATE.

## Tag ancestry

**Problem and cause.** The tag workflow accepted a version-shaped tag on any commit. Its
CI and draft-publication dependencies did not require that commit to have passed through
the protected `master` branch, so a tag on `feature/first-release` could have published a
release before the PR merged.

**Change.** `.github/workflows/release.yml` now fetches `master` after checking out the tag
and calls `scripts/require-release-commit.sh`. The script rejects a tagged commit that is
not an ancestor of `origin/master`. `tests/release_tag_guard.sh`, registered in CTest,
creates a temporary Git history with an RC tag on an unmerged feature commit. The same
tag is accepted after a fast-forward merge. The test failed with an always-accepting
guard (`unmerged release tag was accepted`) and passed with the new guard. `make test`
passed 14/14 suites locally, including this regression.

**v0.1 impact.** A release tag from an unmerged branch stops in `validate`, before the
source archive, app build, full CI and publication jobs. The first real tag run remains
to be checked at `v0.1.0-rc.1`; no release tag was created here.

## Installed release tree

**Problem and cause.** `scripts/ci-run.sh release` built and tested the source tree, then
installed it under `stage/usr`, but never launched that installed binary. The separate CI
GUI smoke uses `build/release/omamovie`, so a missing installed QML file or launch failure
could pass every pre-publication job.

**Change.** The release task now verifies the staged executable, QML, icon and MIME file,
validates the desktop entry, and runs `stage/usr/bin/omamovie --smoke` as an ordinary user
under headless sway/lavapipe. A failure exits the app job before the tarball is uploaded;
the `publish` job already needs the app job and full CI.

**Reproduction and result.** The first local invocation against the working checkout
stopped at CMake configure because its existing `build/release/CMakeCache.txt` named the
host path, while Docker mounts the checkout at `/src`. Repeated from a clean `git archive`
copy of `86b30bd` with the changed release task, `bash scripts/ci-run.sh release` passed
13/13 Release CTest suites in Arch Linux, installed to `stage/usr`, and finished with
`GUI smoke: PASS` from that installed binary. The smoke reported llvmpipe/Mesa 26.2.4,
software H.264/HEVC decode, a libx264 export through the Vulkan compositor, and passing
edit, keyboard, recovery and device-loss checks. The clean archive preceded the new Git
ancestry test; that test was run in the working checkout's 14/14 local suite.

**v0.1 impact and limit.** A tag cannot publish an app tarball whose installed layout fails
this smoke. This is a clean Arch software-Vulkan check. A native Omarchy install, the M6
T1–T5 human walkthrough, and a smoke of the eventual RC archive remain open.

For the PR gate, [run 39032076887](https://github.com/KitsuneForgering/OmaMovie/actions/runs/39032076887)
passed the complete GCC/Clang, Debug/Release/ASan/TSan, GUI, lint and fuzz matrix and
the `ci` aggregator on `86b30bd`; `conventional commits` passed on the same SHA. The
required contexts must be rechecked after this gate change is pushed.
