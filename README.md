# OmaMovie

OmaMovie is a video editor under development for Omarchy. Its current editor shell lets you import local videos and pictures, arrange clips on a timeline, preview the result, and adjust clip audio. It saves and opens projects and exports the movie to MP4 (H.264 and AAC).

## Try the editor

On an Omarchy or current Arch Linux system, from this repository:

```sh
make deps
make run-gui
```

`make deps` installs the packages declared in [PKGBUILD](PKGBUILD) using `pacman` and may request sudo. `make run-gui` builds and opens the Qt Quick editor. Use **Import** or `Ctrl+I` to choose a local file, then select it in the library and append it to the storyline. You can split at the playhead with `Ctrl+B`, undo with `Ctrl+Z`, and play or pause with Space.

You can also open a file directly:

```sh
make run-gui GUI_FILE=/path/to/video.mp4
```

The imported media stays on disk and is never modified. Save with `Ctrl+S` (an `.omamovie` file) and reopen it later with `Ctrl+O`. `Ctrl+K` opens the command palette, which lists every action and lets you change its shortcut. `Ctrl+E` exports the movie as MP4: H.264 at the project's size and frame rate with AAC sound, using the GPU's encoder on Intel (VA-API) and x264 elsewhere (Settings can choose).

## What works today

- Import and inspect local video and still image files through FFmpeg. Actual codec support depends on the installed FFmpeg build and the file.
- Append, insert, overwrite, split, trim, delete, and ripple edit clips with undo and redo.
- Preview the timeline with Vulkan compositing and audio clock playback; adjust clip gain, fades, and mute.
- Use visible transport and edit controls, or their keyboard shortcuts, in the editor shell.

The viewer decodes in hardware on Intel (VA-API) and in software elsewhere; `OMA_PREVIEW_HARDWARE=0` forces software. External project interchange (OTIO, FCPXML) is planned work, not a current capability.

## Install

`makepkg -si` builds the package from the [PKGBUILD](PKGBUILD) and installs `omamovie`, its launcher entry, icon and the `.omamovie` file type. The editor follows the active Omarchy theme and font while it runs.

### Full opacity on Hyprland

Omarchy makes ordinary windows slightly translucent, which blends the wallpaper into the viewer. OmaMovie's window class (Wayland app_id) is `omamovie`. To keep it opaque, as Omarchy does for DaVinci Resolve, add this to `~/.config/hypr/windows.lua`:

```lua
o.window("^omamovie$", { tag = "-default-opacity", opacity = "1 1" })
```

The package does not install this rule. It uses the Lua window-rule syntax of current Omarchy (`default/hypr/apps/davinci-resolve.lua`); older Hyprland configurations need their own syntax. Full opacity only stops the blending: it does not calibrate the display.

## Build and development

Run `make test` for the library tests or `make help` for available targets. The [implementation plan](Docs/implementation-plan.md) tracks completed and pending milestones. [UI design](Docs/ui-design.md) describes the editor workflow. [Format boundaries](Docs/format-architecture.md) and [CLAUDE.md](CLAUDE.md) cover architecture and contributor rules.

License: [MIT](LICENSE).
