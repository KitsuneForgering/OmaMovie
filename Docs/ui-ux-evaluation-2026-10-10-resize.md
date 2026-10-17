## Resize and screen-switch follow-up — 2026-10-11

The reported symptom is transient flicker inside the OmaMovie window during repeated
resizes or screen switches. The GUI smoke now cycles 24 window sizes, Projects/Editor and
Settings, then jitters across the 1500 px layout breakpoint. It checks that the viewer and
timeline recover and that the layout does not flip back and forth inside the hysteresis band.
The run passed on the Intel/Wayland target. The timeline now coalesces its two visible-range
notifications into one model sync per event loop, and the library layout keeps its current
mode within 1450–1550 px. A separate minimap paint error was found: the connected-clip
outline used undefined coordinates when repainting, and it now uses minimap coordinates
and repaints when the followed clip changes. These changes remove concrete sources of
intermediate repaint and layout churn; the smoke verifies the final rendered state, not every
frame visible to the compositor during a manual resize.

[Return to ui-ux-evaluation-2026-10-10.md](ui-ux-evaluation-2026-10-10.md)
