## Discriminating gate before a workaround

On the same display, media fixture, tiling layout and resize script, collect:

1. A 60 fps compositor recording of the OmaMovie rectangle during 30 repeated
   half-width/full-width changes, 20 Projects/Edit/Settings switches and 10
   show/hide cycles. Keep the full recording and time-stamped event list. Inspect
   every visible flash and label it: stretched old surface, blank entire app,
   blank viewer only, stale UI, or unrelated layer. The current `gpu-screen-recorder`
   and `ffmpeg` are available; use a private fixture and avoid recording other
   windows or personal content.
2. Qt `frameSwapped`, `sceneGraphInvalidated`, `sceneGraphError`, window exposure,
   size and device-pixel-ratio timestamps; viewer composite success/failure and
   frame ID; Hyprland window geometry and layer-zone changes. Correlate by a
   monotonic clock. Count gaps over two display periods (about 33 ms at 60 Hz)
   during an expected redraw, and count blank/stale screen frames separately.
3. Repeat with hardware preview decode forced off (`OMA_PREVIEW_HARDWARE=0`) and
   on, a Qt-only Vulkan sample window, and one other Wayland compositor if
   available. Compare the same workload, not arbitrary normal use. Repeat with
   and without Quickshell **only after arranging a reversible session-local test**;
   do not alter the user's shell or window rules as part of the app test.

Accept an app-side fix only when the recorded symptom disappears or materially
improves in repeated A/B runs and the same edits, playback and UI layout still
pass. Stop and classify as compositor-side if app commits are timely and correct
but only Hyprland's displayed surface flashes, with an independent Qt client
showing the same pattern. If shell presence changes the result, inspect its
specific layer geometry before proposing any rule. Full opacity controls wallpaper
blending; it has no demonstrated effect on this resize symptom.
