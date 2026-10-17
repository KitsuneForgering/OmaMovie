# Installed package and PR gate — 2026-10-16

**Classification:** QA GAP / RELEASE GATE.

The first clean Arch `scripts/package-check.sh` run failed in `makepkg`'s test stage:
12/13 CTest suites passed, but the compositor effect-memory test reported an impossible
`+17592186044416 MiB` on lavapipe. The test subtracted unsigned
`VK_EXT_memory_budget.heapUsage` samples; the budget also includes driver variation between
renders. The same test failed locally with `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`
before the fix. The compositor now reports its own live blur/sharpen image allocation sizes,
queried from the images' Vulkan memory requirements. The test passed on lavapipe and Intel
afterward, with 0 MiB added by looks and 5.0, 2.6 and 31.6 MiB for the sampled detail cases.

The second clean package run used committed source `0f2e209`:

```sh
scripts/package-check.sh /tmp/omamovie-package-v01-fixed
```

`makepkg` built `omamovie-git-r527.0f2e209-1-x86_64.pkg.tar.zst`, ran 13/13 CTest suites,
and installed it in a fresh Arch container with `vulkan-swrast`. Before launch the script
checked the installed binary, QML, desktop entry (`desktop-file-validate`), icon, MIME
registration (`gio` recognized `application/x-omamovie-project`), and `ldd` resolution.
The installed `/usr/bin/omamovie --smoke` under headless sway/lavapipe reported
`GUI smoke: PASS` and `smoke exit 0`. Logs and package are in
`/tmp/omamovie-package-v01-fixed/` on the test host.

This proves the clean Arch package and software Vulkan workflow represented by the GUI smoke.
It does not replace the M6 human walkthrough T1–T5 on a fresh Omarchy profile, a native
installed Intel run, or the final release tarball smoke after `v0.1.0-rc.1`.

PR [#4](https://github.com/KitsuneForgering/OmaMovie/pull/4) was opened as a draft against
`master` to exercise the active ruleset, whose required contexts are `ci` and
`conventional commits`. A defective CI run failed on Clang/clang-tidy diagnostics, and its
aggregate `ci` check failed too. After those fixes, run `38985319118` on `041ecb9` passed all
eight GCC/Clang build variants, GUI smoke, format, tidy, include rules, eight fuzz jobs and
`ci`; run `38985319129` passed `conventional commits`. The active `master` ruleset requires
the exact two contexts with strict status checks, and GitHub reported the PR merge state
`CLEAN` at that SHA. Recheck both contexts after the final branch commit.

The host-native Release app was installed to `/tmp/omamovie-host-stage/usr` and launched from
that staged tree on Intel. Its first GUI smoke attempt failed the play/J/K/L position checks
with audio running; a second attempt passed. This is an unresolved intermittent native smoke
observation, so this run is not evidence of a stable Intel GUI workflow. The Arch package from
the newer rolling snapshot also could not launch on this older host because it requires
`libsimdjson.so.34` while the host has `.33` (and different Qt minor versions). The clean
Arch container passed with its matched runtime libraries; Omarchy package validation must
use a compatible, updated environment.
