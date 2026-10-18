# Project analysis — 2026-10-07

Revision `1ecf3b5` (feature/first-release), read-only analysis of the code, tests and CI with
grep checks of the CLAUDE.md invariants; not every file was audited. Follows the
[2026-10-04 analysis](project-analysis-2026-10-04.md).

## Shape

Desktop app (Qt Quick + Vulkan) over layered C++ libraries: libraries 12.8k lines, tests 7.8k,
app 11.4k, of which `session.cpp` is 3,083 lines (96 `Q_INVOKABLE`s, ~174 functions) and
`main.cpp` 2,127 (mostly the GUI smoke).

## Sound

- CLAUDE.md invariants hold: FFmpeg only in `libs/media` (plus a spike in `tools/`), no Qt in
  libraries, no `std::thread` outside `libs/base`; the Makefile refuses forbidden dependencies.
- Format robustness: versioned native format with tested migrations, bounded and fuzzed parser,
  atomic saves with failure injection, measured recovery after `kill -9`.
- Stale asynchronous results are dropped through `generation_`; the job pools are destroyed
  before the data their jobs touch.

## Findings and what was done

1. **Confirmed (code reading, then a test): an export held up the editor.** Every Session job,
   export included, shared one worker (`oma::JobPool workers_{1}`), so saving, autosave,
   imports and the software viewer waited for the whole export. Fixed: exports run on their own
   one-thread pool (they own their decoders and device). The GUI smoke saves while an export
   runs; it fails with the old single worker and passes now.
2. **Confirmed: CI never compiled the app.** `ci.yml` built and tested the libraries only.
   Fixed: debug and release builds also compile `apps/omamovie`. Doing so found a lambda
   capture clang rejects with `-Werror` (fixed).
3. **Maintainability: `Session` has grown into the place every feature lands** (library and
   import, playback, project/autosave/versions, export, source viewer, edits). The timebase bug
   in the source viewer and the export-before-reimport race appeared at these seams. Plan:
   extract units that already have boundaries when they are next touched (export state,
   autosave/versions, source viewer), each with its own tests; no rewrite.
4. **Test gap: app logic is exercised only by one sequential GUI smoke.** A failing step
   cascades, and the smoke is sensitive to window focus and visibility (several intermittent
   failures this week: a click during a panel animation, a hidden window drawing no frames).
   Plan: headless `Session` tests (`QGuiApplication` with the offscreen platform) for pure
   rules such as `sourceFor`, the ripple preview and the export guards.
5. **Theoretical, measured acceptable at the gate: ripple previews copy the timeline on every
   drag step** (O(clips), 5,000 clips at the long-form gate). Skipping steps whose frame offset
   did not change is trivial; not measured at scale.

## Before v0.1 (product gaps)

Install on a clean Omarchy machine; the first release tag (the maintainer's call); the M6
user-task comparisons (UX-10, placement, the VEGAS pilots); NVENC/AMD on real hardware.
