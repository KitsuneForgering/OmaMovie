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
