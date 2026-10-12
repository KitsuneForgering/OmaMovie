# OmaMovie resize flicker on Hyprland and Quickshell

Research and local check, 2026-10-11. This is a diagnostic record, not a confirmed
root cause or a recommendation to change the user's compositor configuration.

## Decision and competing explanations

Treat the reported flicker inside the OmaMovie window during repeated resizing
and screen changes as an open release defect. Three mechanisms can produce similar
symptoms:

1. **OmaMovie/Qt presentation gap.** Its Qt Quick Vulkan swapchain may not present
   a new-size frame promptly; its borrowed viewer image or shared-device queue may
   also fail or stall. Prediction: a captured application surface contains a blank,
   old-size or stale viewer frame, or Qt frame-swap timestamps show a gap correlated
   with the event. The same defect may appear under another Wayland compositor.
2. **Hyprland texture expansion.** The compositor may scale the old surface while
   waiting for the client to commit a new-size buffer. Prediction: the screen
   recording shows stretched or flashing borders while the application's new
   surface, once committed, has correct content. A different Qt app under the same
   tiling resize should show a related effect. A Hyprland maintainer describes
   this behavior in [discussion 14399](https://github.com/hyprwm/Hyprland/discussions/14399)
   (May 2026); that report is an analogy, not proof about OmaMovie.
3. **Shell-layer geometry.** Quickshell's `PanelWindow` can reserve an
   `exclusiveZone` in the compositor. A bar or overlay geometry change can resize
   tiled clients independently of their content. Prediction: the effect tracks
   layer-zone changes and differs in an otherwise identical run without the shell
   layer. See [Quickshell PanelWindow](https://quickshell.org/docs/v0.2.1/types/Quickshell/PanelWindow/)
   and the separate, unresolved [Hyprland report 15177](https://github.com/hyprwm/Hyprland/discussions/15177).

These explanations can coexist. A visible flash alone does not identify which
surface was late.

## Local observations

- Installed: Hyprland 0.56.2, Qt 6.11.2, Quickshell 0.3.1, Mesa 26.2.2, Intel Iris Xe;
  one 1920×1080, scale-1, approximately 60 Hz `eDP-1` monitor. `hyprctl layers -j`
  shows `omarchy-background` and `omarchy-bar`. A process listing outside the
  sandbox confirms `quickshell -n -p .../omarchy-v4.0.4/shell`. An earlier sandboxed
  process query returned an empty result and was discarded.
- `make run-gui RUN_GUI_SMOKE=1` passed on this desktop. The smoke performs 24
  large resizes with Projects/Settings switches and 24 more around the layout
  breakpoint. It checks the final layout and one nonempty `grabWindow()` image.
  Its keyboard check was skipped because the window did not retain focus. This
  is evidence of recovery, **not** absence of transient flicker or present stalls.
- An opt-in `OMA_PRESENT_TRACE=1` run outside the sandbox passed the same smoke,
  including keyboard focus. It recorded 448 queued-frame events, 58 width changes,
  23 height changes and no scene-graph error. The trace records event times in
  microseconds since window creation. This still does not identify what was
  actually displayed by Hyprland. The first traced attempt aborted in the
  sandbox before the application emitted events; it is not counted as an app
  failure. Reproduce the trace with
  `OMA_PRESENT_TRACE=1 make run-gui RUN_GUI_SMOKE=1 2> /tmp/omamovie-present-trace.log`.
  Per-frame stderr logging perturbs timing, so compare a trace run with an
  untraced capture before treating a small latency difference as meaningful.
- The app shares one Vulkan device between Qt Quick, its compositor and optional
  hardware decode (`main.cpp`, `PreviewItem`, ADR-0005). The bridge gates workers
  around Qt frames and the viewer wraps the compositor's `VkImage` in a
  `QRhiTexture`. This is a plausible failure boundary, not a demonstrated fault.
  Qt documents that Vulkan normally uses a threaded scene graph and that
  [`frameSwapped()`](https://doc.qt.io/qt-6.8/qquickwindow.html) means queued for
  presentation. Qt's [swapchain guidance](https://doc.qt.io/qt-6.11/qrhiswapchain.html)
  says surface pixel size may differ from window size during resize. OmaMovie lets
  Qt manage its own swapchain, so it should not attempt to recreate it from
  `QWindow::size()` itself.
- The existing `grabWindow()` smoke cannot serve as a continuous presentation
  oracle: Qt warns it is costly, and ADR-0005 records validation hazards from
  capture on the shared Vulkan device. A compositor-level recording and
  application frame timestamps are better complementary observations.

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

## Coverage and next action

This pass inspected local code, installed component versions, native smoke
runs, Qt/Quickshell documentation and two Hyprland reports. It did not capture the
reported flicker, run the A/B matrix, or prove a cause. The cheapest decisive next
observation is a private compositor recording aligned with Qt frame and window
events during the existing resize stress. Add those diagnostics to a dedicated
resize audit before changing the rendering path or Hyprland configuration.
