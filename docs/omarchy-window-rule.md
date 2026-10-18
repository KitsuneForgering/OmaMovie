# Full opacity on Hyprland

Omarchy makes ordinary windows slightly translucent, which can blend the wallpaper into OmaMovie's viewer. OmaMovie's window class (Wayland app ID) is `omamovie`. To keep the editor opaque, add this rule to `~/.config/hypr/windows.lua`:

```lua
o.window("^omamovie$", { tag = "-default-opacity", opacity = "1 1" })
```

The package does not install this rule. It uses the Lua window-rule syntax of current Omarchy; older Hyprland configurations need their own syntax. Full opacity stops wallpaper blending but does not calibrate the display.
