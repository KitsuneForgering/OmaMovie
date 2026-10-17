# OmaMovie

<img src="Docs/media/omamovie-logo.svg" width="360" alt="OmaMovie logo: a play button over three timeline clips">

OmaMovie is a video editor under development for Omarchy. Its current editor shell lets you import local videos and pictures, arrange clips on a timeline, preview the result, and adjust clip audio. It saves and opens projects and exports MP4, Matroska, or WebM movies.

![OmaMovie editor showing a preview, video clips, and an audio lane on the timeline](Docs/media/editor-test.webp)

*The running editor with synthetic test clips. The preview and timeline are captured from the GUI smoke test.*

## Try the editor

On an Omarchy or current Arch Linux system, from this repository:

```sh
make deps
make run-gui
```

`make deps` installs the packages declared in [PKGBUILD](PKGBUILD) using `pacman` and may request sudo. `make run-gui` builds and opens the Qt Quick editor.

For a first edit:

1. Choose **New project**, then **Import** (`Ctrl+I`). Files imported while the timeline is empty are added to the storyline automatically. Once it has clips, new imports stay in the media library until you place them.
2. Select a library item and use **Add** (`E`) to append it. To use only part of it, open the source viewer (`Shift+O`), set In (`I`) and Out (`O`), then use **Add**. Escape returns to the sequence viewer.
3. Play or pause with Space, split at the playhead with `Ctrl+B`, and undo with `Ctrl+Z`. Save with `Ctrl+S` and export with `Ctrl+E`.

You can also open a file directly:

```sh
make run-gui GUI_FILE=/path/to/video.mp4
```

The imported media stays on disk and is never modified. Projects are saved as `.omamovie` files and can be reopened with `Ctrl+O`. `Ctrl+K` opens the command palette, which lists every action and lets you change its shortcut. `Ctrl+E` offers the output formats supported by the installed FFmpeg build: MP4 (H.264/AAC), Matroska (H.264/AAC or HEVC/AAC), and WebM (VP9/Vorbis). H.264 can use the GPU encoder on validated Intel hardware (VA-API), with a software fallback. Other profiles use software encoders. All outputs use the project's size and frame rate.

## What works today

- Import and inspect local video, audio and still image files through FFmpeg. Import checks that the chosen video and audio streams can produce an initial frame or samples; later damage in a file may still cause a decode error.
- Append, insert, overwrite, split, trim, delete, and ripple edit clips with undo and redo.
- Preview the timeline with Vulkan compositing and audio clock playback; adjust clip gain, fades, and mute.
- Use visible transport and edit controls, or their keyboard shortcuts, in the editor shell.

The viewer decodes in hardware on Intel (VA-API) and in software elsewhere; `OMA_PREVIEW_HARDWARE=0` forces software. The app has bounded OTIO, FCPXML, EDL and MLT import paths. See the [interop matrix](Docs/interop.md) for implemented subsets and evidence gaps; these are not general compatibility claims for every project made by those editors.

## Install

`makepkg -si` builds the package from the [PKGBUILD](PKGBUILD) and installs `omamovie`, its launcher entry, icon and the `.omamovie` file type. The editor follows the active Omarchy theme and font while it runs.

### Full opacity on Hyprland

Omarchy makes ordinary windows slightly translucent, which blends the wallpaper into the viewer. OmaMovie's window class (Wayland app_id) is `omamovie`. To keep it opaque, as Omarchy does for DaVinci Resolve, add this to `~/.config/hypr/windows.lua`:

```lua
o.window("^omamovie$", { tag = "-default-opacity", opacity = "1 1" })
```

The package does not install this rule. It uses the Lua window-rule syntax of current Omarchy (`default/hypr/apps/davinci-resolve.lua`); older Hyprland configurations need their own syntax. Full opacity only stops the blending: it does not calibrate the display.

## Build and development

Run `make test` for the library tests or `make help` for available targets. The [implementation plan](Docs/implementation-plan.md) tracks completed and pending milestones. [UI design](Docs/ui-design.md) describes the editor workflow. [Format boundaries](Docs/format-architecture.md), [contribution guide](CONTRIBUTING.md) and [agent guide](CLAUDE.md) point to architecture and development rules.

License: [MIT](LICENSE).
