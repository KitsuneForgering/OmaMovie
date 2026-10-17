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
`conventional commits`. The latter passed on the PR. The first full CI run blocked the PR on
Clang and clang-tidy diagnostics; these were addressed in later branch commits. The aggregate
`ci` check and merge gate must still be observed on the final SHA before marking them passed.
