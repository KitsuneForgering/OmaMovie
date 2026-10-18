## Coverage and next action

This pass inspected local code, installed component versions, native smoke
runs, Qt/Quickshell documentation and two Hyprland reports. It did not capture the
reported flicker, run the A/B matrix, or prove a cause. The cheapest decisive next
observation is a private compositor recording aligned with Qt frame and window
events during the existing resize stress. Add those diagnostics to a dedicated
resize audit before changing the rendering path or Hyprland configuration.
