# Local sanitizer pass on feature/first-release — 2026-10-22

**Classification:** CORRECTNESS (local stand-in for CI). PR #4 is merged and `ci.yml` runs on
pull requests and pushes to `master` only, so the commits pushed to `feature/first-release` on
2026-10-21/22 (`5706848`…`089f99f`) have not run in CI. Until a PR does, this is the record.

Host: i5-1135G7 / Iris Xe. Build presets `debug`, `asan`, `tsan` (c++ (GCC) 16.2.1 20260810).
CI's test containers have lavapipe (`vulkan-swrast` in the PKGBUILD's checkdepends) and no
VA-API, so the runs used `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` and
`LIBVA_DRIVER_NAME=oma_bogus`.

| Run at `089f99f` | Result |
|---|---|
| `ctest --preset debug` (Iris Xe, VA-API) | 15/15 |
| `app_tests` debug, lavapipe, no VA-API | 136/136 suites |
| `ctest --preset asan`, lavapipe, no VA-API | 15/15 (240 s) |
| `ctest --preset tsan`, lavapipe, no VA-API | 15/15 (195 s) |

Not covered: CI's second compiler, the clang-tidy and fuzz jobs, the GUI smoke under sway,
`vm.mmap_rnd_bits=28` for TSan (not changeable here; the run passed without it).
Without any Vulkan device (`VK_ICD_FILENAMES=/nonexistent.json`) five older app tests that
export 1080p in debug exceed their 60 s wait on the CPU compositor; no CI job runs that way.
*Update 2026-10-23*: after the CPU compositor's one-tap and threaded-rows changes the three
1080p exports finish in 16–32 s, and the two decode-path tests failed only because they
required the reason "chosen in Settings" where a machine without a device says "no GPU
device"; they accept both now. The whole `app_tests` suite passes without any Vulkan device
(136/136, 145 s).

**Follow-up at `f352aa3`.** The later playback and compositor changes (frame
counters in `VideoScheduler`, `PreviewAdapt`, unspecified chroma read as left) were run
under the same sanitizer environment ctest gives: TSan on the scheduler, adaptive-preview and
seeded Session stress tests (all pass, no TSan report) and ASan on `compositor_tests` (258/258)
and the scheduler test (pass).
