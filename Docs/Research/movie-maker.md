# Microsoft Movie Maker (and successors): UI/UX

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Items marked **(general observation)** come from broad knowledge of the product without a
> specific source in this research.
>
> Covers Windows Movie Maker (2000–2009), Windows Live Movie Maker (2009–2017) and, as context,
> its successors: the Photos app Video Editor and Clipchamp.

---

## 1. Summary

For many people Movie Maker was their first video editor. The useful lessons are:

1. **Storyboard and timeline as two views of the same project.** In the last version (2012)
   they became one area with **zoom**: zoomed out it is a storyboard; zoomed in it is a timeline.
2. **A task-guided flow** (capture → edit → publish).
3. **AutoMovie**: predefined styles that assemble a movie automatically.
4. Negative lessons: **narrow format support** in the first version and **features removed**
   between versions, the same mistake as iMovie '08.

---

## 2. History

| Date | Version | Notes |
|---|---|---|
| 2000-09-14 | 1.0 (Windows Me) | Criticized for its small feature set and for exporting only ASF |
| 2001 | 1.1 (Windows XP) | DV-AVI and WMV 8 export |
| 2002-11 | **2.0** | Large feature expansion; 60+ transitions and 37 effects; custom XML effects |
| 2004–2005 | 2.1 (XP SP2), MCE 2005 | DVD burning in Media Center Edition |
| 2007 | **6.0 (Vista)** | DirectX/Direct3D effects, HDV; **removed analog capture** and XML customization. Version 2.6 released for older hardware |
| 2009-08-19 | **Windows Live Movie Maker** | Rewrite: **ribbon** toolbar (Office 2007 style), **AutoMovie**, direct publishing to YouTube/Facebook. **Removed** stabilization and narration |
| 2010-08 | 2011 | Brought back HD video and webcam capture |
| 2012-04 | 2012 | H.264/MP4 as the default export; narration back |
| 2017-01-10 | — | **Discontinued** |
| Windows 10 | Video Editor (Photos app) | Replacement, with text-to-speech and OneDrive |
| 2021-09-08 | **Clipchamp** | Acquired by Microsoft; Windows 11's default editor; brings back the multitrack timeline |

In 2013 PCMag called it "possibly the simplest (and funnest) way to combine video clips".

---

## 3. Classic interface (versions 2.x–6.0)

- **Task pane** on the left, organized in steps (capture video, edit movie, finish movie)
  **(general observation; Wikipedia describes the layout in general terms)**.
- **Collections**: organization of imported media.
- Real-time **preview**.
- **Storyboard / Timeline** at the bottom, switchable:
  - **Storyboard**: large fixed-size boxes for each clip or image, with smaller boxes between
    them for transitions. Used to order the material.
  - **Timeline**: each rectangle's length is proportional to its duration; supports trimming,
    adjusting transition duration and shows the audio track.
  - Separate tracks for video, audio/music and titles.

### What worked

- A storyboard is great for the first assembly (scene order) and is less intimidating.
- A transition as a visible object **between** two clips, with its own box, makes the concept obvious.
- The step-by-step flow taught the editing process.

### What did not work

- Switching modes is a context switch: users must know which mode they are in to understand what they can do.
- The track set was fixed and limited.

---

## 4. Windows Live Movie Maker (2009–2012)

- A **ribbon** instead of menus, with tabs per category (Home, Animations, Visual Effects,
  Project, View) **(general observation for the tab names)**.
- **A hybrid storyboard/timeline area with a zoom slider** in the bottom-right corner. With the
  slider all the way left, each clip is a same-size rectangle (storyboard). Zooming in, the
  rectangles start reflecting duration (timeline).
- **AutoMovie**: pick a style and the app builds the movie with title, credits, transitions and music.
- **Direct publishing** to platforms (YouTube, Facebook).
- Simplified editing: one main video track, no free multitrack **(general observation)**.

### Implications for OmaMovie

- **Semantic zoom** is the most reusable idea in this research. A single timeline component
  with a zoom axis that goes from "storyboard" (one thumbnail per clip, uniform size, focus on
  order) to "timeline" (proportional to time, focus on cuts). No separate mode and no context switch.
  - Technical requirement: the TimelineView needs a layout that is not linearly scaled at the
    zoomed-out extreme. The timeline model does not change; only the visual projection does.
  - Needs measuring: thumbnails for every clip at minimum zoom require a cache and list virtualization.
- The ribbon is an Office pattern, not a video editor pattern. Do not adopt it; the contextual
  inspector plays the same role with less fixed space.
- AutoMovie and templates: same conclusion as iMovie (`imovie.md` §7): produce a normal, editable timeline.

---

## 5. Successors

### Photos Video Editor (Windows 10)

Replaced Movie Maker with an even simpler, storyboard-based approach, with text-to-speech and
OneDrive integration. Wikipedia notes that timeline-based editing only returned with Clipchamp,
so Windows went years without a built-in timeline editor.

### Clipchamp (Windows 11)

A three-column layout plus a timeline:

- **Left sidebar with tabs**: Your media, Record & create (webcam, screen, text-to-speech),
  Templates, Music & SFX, video/image libraries (stock), Graphics, Text, Transitions, Brand kit.
- **Preview** in the center.
- **Multitrack timeline** at the bottom.
- **Contextual properties panel on the right**: changes with the selected element, with tabs
  for color, audio, filters, speed, transitions and text.
- Templates by target format (Instagram, TikTok, etc.) and AI-generated videos.
- Export at several resolutions (4K on the paid plan) and direct upload to social media.
- Reviews describe the interface as "refreshingly simple" and similar to Premiere in arrangement.

### Implications for OmaMovie

- Clipchamp arrived at the same layout OmaMovie plans: library on the left, preview in the
  center, contextual inspector on the right, timeline at the bottom. That convergence validates
  the design in `CLAUDE.md` §11.
- **A library with tabs per content type** (media, text, transitions, audio) is an alternative
  to iMovie's "single browser". For OmaMovie, the version without stock content:
  Media / Titles / Transitions / Effects.
- **Integrated screen/webcam recording** is a feature with real demand in simple editors. On
  Omarchy/Wayland it would go through PipeWire and xdg-desktop-portal (ScreenCast). Not a
  priority; record it as a future idea.
- Technical contrast: Clipchamp is built on web technology **(general observation)**. OmaMovie,
  native and GPU-first, should compete on performance and on working offline without an account.

---

## 6. Negative lessons

| Mistake | Version | Lesson for OmaMovie |
|---|---|---|
| Exported only ASF | 1.0 | Broad format support is part of usability. FFmpeg solves it; do not restrict artificially |
| Removed analog capture | 6.0 | A platform change does not justify losing capability |
| Removed stabilization and narration | Live 2009 | A rewrite that removes features causes rejection (same as iMovie '08) |
| Removed effect customization | 6.0 and Live | Withdrawn extensibility does not come back. Do not promise extensibility (plugins) before it can be maintained, as `CLAUDE.md` already says |
| Discontinued without an equivalent successor | 2017 | Users spent years without a timeline on Windows. An architecture that needs a full rewrite tends to kill the product; hence the "grow without rewriting the core" rule |

An internal memo from Bill Gates (January 2003), revealed in an antitrust case, complained about
how hard it was to download and install Movie Maker. **Lesson:** the first experience (install,
open, import) is part of the product. For OmaMovie: simple packaging for Arch/Omarchy and a
first run with no configuration.

---

## Sources

- [Wikipedia: Windows Movie Maker](https://en.wikipedia.org/wiki/Windows_Movie_Maker)
- [InformIT: Using Microsoft Movie Maker 2](https://informit.com/articles/article.aspx?p=370630)
- [USF Tech Ease: What is Windows Movie Maker](https://etc.usf.edu/techease/win/images/what-is-windows-movie-maker/)
- [O'Reilly: Customizing your Timeline view (Windows Movie Maker 2)](https://www.oreilly.com/library/view/microsoft-windows-movie/0321199545/0321199545_ch09lev1sec5.html)
- [XDA: How to use the Clipchamp video editor on Windows 11](https://www.xda-developers.com/how-to-use-clipchamp-video-editor-windows-11/)
- [ITPro: Clipchamp to be installed on Windows 11 by default](https://itpro.com/operating-systems/microsoft-windows/366289/clipchamp-video-editor-to-be-pre-installed-on-windows-11)
- [Help Desk Geek: How to use the Windows 11 video editor (Clipchamp)](https://helpdeskgeek.com/how-to-use-the-windows-11-video-editor-clipchamp/)
- [Windows Report: Clipchamp review](https://windowsreport.com/clipchamp-review/)
