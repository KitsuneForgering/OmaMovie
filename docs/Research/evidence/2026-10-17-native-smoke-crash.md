# Native installed smoke: crash and false failure — 2026-10-17

**Classification:** QA GAP / BUG in the smoke driver.

The staged Release binary from `86b30bd` exited with SIGSEGV on Intel/Omarchy during
`--smoke`. `coredumpctl info 4009459` records the command and signal. Symbolizing that
core identified `QVariant::toMap()` called from `gui_smoke.cpp:1785`, at
`session.clips().front()` in the export step. At the crash, the smoke's prior launch and
autosave checks had already failed, `clips_after_edit` was zero, and the restored timeline
was empty. The empty list access caused the SIGSEGV; the earlier reason for zero clips
was not recovered from this core. The core copy used for GDB was deleted afterward.

`apps/omamovie/src/gui_smoke.cpp` now stops with a diagnostic if the forwarded-file
step fails or if there are no clips before autosave or export. A failing prerequisite
will fail the smoke instead of producing an unrelated crash. A later native run with
those guards produced a separate, actionable false failure at Save from the discard
dialog: the test read `session.dirty()` *after* clicking Save. The captured state was
`dirty 0`, `media 0`, empty project path, showing that save and New Project had already
completed. The check now records the dirty precondition before clicking; the following
step still verifies that New Project completed, and the reopen check verifies that the
saved edit survived.

**Checks:** `make format-check` and `make test` passed (14/14 suites). The installed
Release binary in `/tmp/omamovie-host-stage/usr/bin/omamovie` passed three consecutive
native Intel GUI smokes after the check change, including keyboard, save/reopen,
autosave/recovery and VA-API export. The software-Vulkan installed smoke also passed in
the [clean Arch release-task run](2026-10-17-release-tag-and-staged-smoke.md).

**v0.1 impact and limit:** the test can no longer turn a failed prerequisite into a
SIGSEGV, and fast successful saves no longer fail the guard check. The origin of the
one empty-timeline run remains unproven; repeat the installed native smoke on the RC
and investigate any new early failure using the added state diagnostics. This is not
evidence that arbitrary native project transitions are free of races.
