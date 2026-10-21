# Export audit hangs on this Wayland session: an environment fault, not OmaMovie — 2026-10-21

**Classification:** RELIABILITY / diagnosis. Host: i5-1135G7 / Iris Xe, Hyprland, Mesa 26.2.4,
Qt 6.12. Release build of `3af6652`.

**Symptom.** `omamovie --export-audit a2160.mp4 out.mp4` sometimes never finished (2 of 7 runs
around 12:50, then every run after 13:20): no CPU use, Hyprland's "Application Not Responding"
dialog, and an earlier run that reported `export audit: 0 frames`. The GPU-native input A/B
(`2026-10-21-export-gpu-input-ab.md`) used only runs that finished; their outputs were
byte-identical.

**Where it waits.** Thread wait points (`/proc/<pid>/task/*/wchan`) and a gdb backtrace of a hung
run: the Qt Quick render thread is in `QRhi::beginFrame` → `libvulkan_intel` →
`drmSyncobjTimelineWait`, Mesa waiting on a sync object before Qt can start the next frame. The
GUI thread waits for that render thread (futex, scene-graph sync). No OmaMovie GPU wait is in
progress (every one of them goes through `gpu::CommandRunner`, and no thread is in it), the
export job had not started, and the kernel log shows no i915 hang or reset.

**Not OmaMovie.** While the audits hung, Khronos' own `vkcube --c 120` (120 frames, then exit)
also never finished, and the offscreen compositor tests still passed. Presentation through
Wayland was blocked for every Vulkan client, while the GPU worked offscreen. The display was
powered off (`hyprctl monitors`: `dpmsStatus false`), and turning it on with
`hyprctl dispatch dpms on` did not unblock new windows within 120 s, so DPMS alone is not the
whole explanation. Stale "not responding" dialogs piled up on workspace 1 meanwhile.
`MESA_VK_WSI_DEBUG=nowlts` (no Wayland explicit sync) hung the same way in that state, so that
experiment does not discriminate. Its earlier clean comparison was cut short.

**Consequences.**
- Window-based checks (`--export-audit`, `--m6-timeline-audit`, the GUI smoke) need a session
  that presents: an active display and a compositor that releases buffers. Before blaming
  OmaMovie for a hang in `QRhi::beginFrame`, run `vkcube --c 120`. If it hangs too, the session
  is at fault.
- Headless measurements (spikes, `tools/bench/import-scaling`, the app's offscreen tests) are
  unaffected.
- A real export runs on its own job and device. Whether it completes while the window cannot
  present was not checked here.

**Process note.** Two hang-hunting tools failed along the way: `pgrep -f` matched gdb and then
the calling shell instead of the program, and gdb 18.1 aborted on an assertion once its debuginfod
cache was populated. The wait points and the `vkcube` comparison were what settled it.
