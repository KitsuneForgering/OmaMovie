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
