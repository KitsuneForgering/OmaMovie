# Apple iMovie: UI/UX

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Items marked **(general observation)** come from broad knowledge of the product without a
> specific source in this research; verify before using them as requirements.
>
> The goal is to extract UX **principles**. `CLAUDE.md` forbids copying iMovie's visual identity
> or proprietary behavior.

---

## 1. Summary

iMovie is simple because:

1. the screen has **three fixed areas** (browser, viewer, timeline) and little else;
2. adjustment controls appear **in context**, in a bar above the viewer;
3. compositing effects are offered as **intent-named operations** (picture-in-picture,
   cutaway, split screen, green screen), not as generic layers and parameters;
4. the timeline is **magnetic**: it leaves no gaps and avoids collisions;
5. there are **templates** (themes, trailers, storyboards, Magic Movie) for those who do not want to start from scratch;
6. underneath, since 2013, it uses the **Final Cut Pro X engine**, and a project can be sent to Final Cut Pro.

Item 6 is the most relevant for OmaMovie: it is exactly "simple UI, serious pipeline".

---

## 2. History relevant to UX

| Year | Version | Change |
|---|---|---|
| 1999 | iMovie 1 | Launched with the iMac DV |
| 2004 | iMovie 4 | Non-destructive editing ("Direct Trimming") |
| 2005 | iMovie HD (5/6) | HDV support |
| 2007 | **iMovie '08** | Full rewrite by Randy Ubillos focused on speed. Introduced **skimming** and **Events** (clips grouped by day). **Heavily criticized**: removed timeline audio and plugins. David Pogue called it "an utter bafflement" |
| 2009–2010 | iMovie '09 / '11 | Brought removed features back; added **trailers** and the **Precision Editor** |
| 2013 | **iMovie 10** | New rewrite on the **Final Cut Pro X engine**; libraries |
| 2015 | 10.1 | 4K editing |
| 2020 | 10.2 | Apple silicon |
| 2022 | iOS 3.0 | **Storyboards** and **Magic Movie** |

### The iMovie '08 lesson

The rewrite brought good ideas (skimming, organization by events) but removed capabilities
users already had. The negative reaction forced Apple to put features back over the next two
versions.

**For OmaMovie:** simplifying the UI **must never mean removing capability** already shipped.
That is the "hide, do not remove" principle of `CLAUDE.md` (§11), with a concrete historical case.

---

## 3. Window layout

According to the official guide, the window has three areas:

- **Browser**: media and content library (transitions, titles, audio, backgrounds).
- **Viewer**: preview, with filters and effects applied.
- **Timeline**: the project sequence, video on top and audio below.

Interaction details:

- **A single divider resizes everything**: dragging the border between viewer and timeline up
  or down adjusts all three areas proportionally.
- The **Libraries List** and the **browser** can be hidden (the "Content Library" toolbar button).
- **Window > Revert to Original Layout** restores the default layout.
- There are no free panels or workspaces.

### Implications for OmaMovie

- The target layout in `CLAUDE.md` (MediaPanel, PreviewPanel, TimelineView, Inspector,
  TransportControls) is similar in spirit. To keep it simple: **few dividers, a fixed layout,
  "restore layout" always available**.
- In Hyprland the user already manages windows in a tiling layout. A single window with a fixed
  internal layout fits that better than floating panels.

---

## 4. Contextual adjustments bar

- The **adjustments bar** sits above the viewer. When a clip is selected in the browser or the
  timeline, its buttons show the corresponding controls.
- Documented example (color): the **Color Balance** button and the **Color Correction**
  button, which shows a multi-slider control, saturation and color temperature.
- Applied state is visible: buttons are highlighted when an automatic adjustment is applied to the clip.
- Interactive tools (e.g. the eyedropper) appear **in the viewer** when needed.
- The guide lists other adjustments of the same kind: auto enhance, crop, rotation, Ken Burns,
  stabilization, filters, speed, volume.

### Implications for OmaMovie

- This is the **contextual Inspector** model of `CLAUDE.md` §11, with one detail to adopt:
  **visually show which adjustments are active on the clip** (badge or highlight) without
  opening each section.
- **Direct manipulation in the preview** (crop, position, eyedropper) beats numeric fields for
  most users. Numeric values should exist in the inspector for precision. Technical implication:
  the PreviewPanel needs interactive overlays that talk to the model through commands (with
  undo), without keeping state of their own.

---

## 5. Timeline and editing

### Magnetic timeline

The concept comes from Final Cut Pro X, whose engine iMovie uses: clips "magnetically" adjust
around a dragged clip, and when a clip is removed its neighbors close the gap. There are no
accidental gaps.

### Trimming methods (documented)

1. **Drag the clip edges** in the timeline.
2. **Clip Trimmer**: shows the unused part of the clip dimmed; lets you extend, shorten or slide
   the used section without changing duration (equivalent to a **slip**).
3. **Precision Editor**: double click an edge; adjusts the start and end of clips and the
   duration of transitions; supports **split edits** (audio and video with different cut
   points, J-cut/L-cut).
4. **Range selection** (hold R + drag) and "Trim Selection".

### Overlays as named operations

Picture-in-picture, cutaway, split screen and green screen are offered as **high-level
effects**: the user places a clip above another and picks the intent. No need to understand
layers, masks or blend modes. Compositing capability is **limited** (no arbitrary multiple
video layers and no general keyframes) **(general observation)**.

### Implications for OmaMovie

- **Magnetic timeline as the default UI behavior**, implemented as ripple in the model. The
  timeline model stays generic (free tracks, gaps allowed); "magnetism" is an editing policy
  applied by the UI's commands.
- **Precision Editor = a friendly UI for roll and split edits.** It shows advanced trimming
  can be simple with good visualization (showing the unused material). The command model in
  `premiere-pro.md` §3 serves both audiences.
- **Named operations compiled to the generic compositor**: "Picture in picture" in the UI
  becomes layers + transform + crop + border in the render graph. OmaMovie can offer iMovie's
  presets **without** its layer limitation, because the compositor is generic. This applies
  "simple UI, serious pipeline" directly.
- The preset must stay editable: after applying "PiP", the inspector shows normal
  transform/crop, and the user can go beyond what the preset provided.

---

## 6. Library and skimming

- **Skimming** (since iMovie '08): hovering over a clip shows its content at a user-controlled
  speed, without pressing play.
- Thumbnail size is adjustable with a slider, as is how much time each thumbnail represents and whether waveforms show.
- Clips can be **rated** (favorite/rejected) and filtered.
- Media is organized into **libraries** and **events**.

### Implications for OmaMovie

- Skimming is the interaction that most sets a "visual" library apart. **It needs
  architecture**: fast decoding of arbitrary frames (seek), a thumbnail cache at several
  densities, and work off the UI thread with aggressive cancellation (the mouse moves faster
  than decode). This feeds the cache (§15) and job system (§13) requirements of `CLAUDE.md`.
- Clip ratings (favorite/rejected) are light metadata. A good case for SQLite (§17).

---

## 7. Templates and automation

- **Themes**: a visual style applied to the titles and transitions of the whole movie.
- **Trailers**: Apple templates per genre, with a shot list and titles; they can be converted into a normal project.
- **Storyboards** (iOS 3.0): ~20 suggested outlines (recipe, Q&A, product review, news report),
  with tips per scene; reorderable and editable.
- **Magic Movie** (iOS 3.0): builds a video from selected clips, identifying dialogue, faces
  and actions, and adds transitions, music and titles.

### Implications for OmaMovie

- The most important point: **a trailer can be converted into a normal project**. Templates
  must produce ordinary timeline structures, not a closed special mode. In OmaMovie a template
  would be a generator of commands/ProjectIR, without a separate project type.
- Templates and automatic generation are not a priority (initial non-goals), but the "template
  → normal timeline" design costs little if planned.

---

## 8. A path to professional tools

- iMovie has used the Final Cut Pro X engine since 2013.
- The guide documents **sending projects to Final Cut Pro**.

### Implications for OmaMovie

- Validates the project's thesis: a simple UI on a professional engine, with no dead end when
  the user grows.
- Difference for OmaMovie: Apple has two apps; OmaMovie wants **one app** in which advanced
  capability is hidden, not moved into another product. That requires more discipline in the
  contextual inspector and in progressive disclosure.

---

## 9. Shortcuts

The guide has a keyboard shortcuts page (window navigation, editing, playback). iMovie is
usable from the keyboard but mouse-oriented **(general observation)**.

**For OmaMovie:** `CLAUDE.md` requires full keyboard editing, which means going beyond iMovie
here, closer to Premiere.

---

## 10. What not to copy

- Visual identity, icons, brand names ("Magic Movie", "Precision Editor") and trade dress.
- **Capability limits** that exist only to keep things simple (fixed number of layers, no
  keyframes). OmaMovie hides complexity; it does not remove it.
- Ecosystem dependence (Photos, iCloud).

---

## Sources

- [Apple: iMovie User Guide for Mac](https://support.apple.com/guide/imovie/welcome/mac)
- [Apple: Change the window layout in iMovie on Mac](https://support.apple.com/guide/imovie/change-the-window-layout-movcab8cb6ef/mac)
- [Apple: Trim clips in iMovie on Mac](https://support.apple.com/guide/imovie/trim-clips-movf8b8fc9b2/mac)
- [Apple: Adjust a clip's color in iMovie on Mac](https://support.apple.com/guide/imovie/adjust-a-clips-color-movf2a3f0f42/mac)
- [Apple: Keyboard shortcuts in iMovie on Mac](https://support.apple.com/guide/imovie/keyboard-shortcuts-movd9d8f91e8/mac)
- [Apple: Send projects to Final Cut Pro](https://support.apple.com/guide/imovie/send-projects-to-final-cut-pro-movcbf7e2a3f/mac)
- [Apple: Final Cut Pro interface (Magnetic Timeline, browser, viewer)](https://support.apple.com/en-vn/guide/final-cut-pro/ver92bd100a/mac)
- [Apple: Intro to the Magnetic Timeline in Final Cut Pro for iPad](https://support.apple.com/it-it/guide/final-cut-pro-ipad/devd9b6a5715/ipados)
- [Wikipedia: iMovie](https://en.wikipedia.org/wiki/IMovie)
- [Engadget: Maybe iMovie '08 isn't such a bad change after all](https://www.engadget.com/2007-08-27-maybe-imovie-08-isnt-such-a-bad-change-after-all.html)
- [Joseph Dickerson: iMovie 08 – a step back?](https://www.josephdickerson.com/blog/2007/08/19/imovie-08-a-step-back/)
- [XDA: Apple iMovie 3.0 introduces Storyboards and Magic Movie](https://www.xda-developers.com/apple-imovie-3-features/)
- [Videomaker: iMovie 3.0 receives two major time-saving features](https://www.videomaker.com/news/imovie-3-0-receives-two-major-time-saving-features/)
