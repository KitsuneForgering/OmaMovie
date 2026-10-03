# Other editors: what people like

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Several sources are review aggregators (G2, Capterra, comparisons); they help identify
> patterns of opinion, not measurements.
>
> Clipchamp is covered in [`movie-maker.md`](movie-maker.md) §5. iMovie, Premiere, Resolve and
> CapCut have their own documents.

---

## 1. Final Cut Pro (Apple)

### What people like
- **Magnetic timeline**: closes gaps automatically when clips are removed and prevents
  accidental misalignment between tracks; fewer common editing errors.
- **Performance**: built for Apple Silicon and Metal; real-time playback of complex timelines;
  render times and timeline responsiveness on multi-stream 4K/HDR projects are reasons cited
  for choosing it over cross-platform alternatives.
- **Background rendering**: rendering happens while the user edits.
- **One-time purchase** (about US$ 300) with free updates, no subscription.

### Implications for OmaMovie
- FCP's performance comes from **optimizing for one specific platform**. That is OmaMovie's bet
  too (Omarchy, Vulkan, VA-API, PipeWire), instead of being cross-platform.
- **Background rendering** = a rendered-frame cache (`CLAUDE.md` §15) filled by low-priority jobs
  when real-time playback is not possible. It must be invisible: the user does not click "render".
- Magnetic timeline: see `imovie.md` §5 (an editing policy, not a model restriction).
- The **FCPXML** format is a high-priority interop target (documented XML).

---

## 2. Descript

### What people like
- **Editing video like a document**: deleting a sentence from the transcript deletes the matching audio/video.
- Intuitive and accessible to people who have never edited video.
- AI tools to improve audio.
- Widely used by podcasters.

### Implications for OmaMovie
- Text-based editing also exists in Premiere (`premiere-pro.md` §7). It is a pattern that is here
  to stay for spoken content.
- Architecturally, the transcript is **one more view over the timeline**: each word has a
  `TimeRange` in the media, and deleting text produces normal editing commands (ripple delete)
  with undo. Not a parallel model.
- Depends on transcription (AI, a non-goal). If it ever lands: a local model, and the data model
  (words with time ranges) should work with any transcription source, including imported ones.

---

## 3. Kdenlive

A free editor (KDE) built on the **MLT** framework. OmaMovie's most direct competitor on Linux.

### What people like
- Free, open source, multitrack, chroma key, color correction, 4K rendering, keyframes,
  customizable workspaces.
- **Privacy and offline use**, no ads or sign-up.

### Current state (2025–2026)
- 2025 with a declared focus on **stability**, timeline performance, audio and captions; closer
  collaboration with the MLT developers.
- 25.04: import a clip straight from the timeline context menu; **zoom toward the mouse**
  instead of the playhead; waveforms for sequences.
- 25.08.x: maintenance releases fixing crashes and regressions.
- **In development**: **10/12-bit** color support, playback optimizations (decoding),
  **OpenFX**, keyframe refactoring with a **dopesheet**.

### Implications for OmaMovie
- Kdenlive is still **adding** 10/12-bit and decode optimizations. OmaMovie can treat high bit
  depth and GPU decode as a foundation from the start (`CLAUDE.md` §7).
- The focus on stability, after years of complaints, shows that **stability is what free editor
  users ask for most**.
- Small UX details worth adopting: **mouse-centered timeline zoom**; importing straight from the timeline.
- A **dopesheet** (keyframes from several effects together) is a good reference for when OmaMovie has keyframes.
- Kdenlive projects are MLT XML: a natural importer candidate (open, text-based format).

---

## 4. Shotcut

A free editor, also built on MLT.

### What people like
- Free and open source, with multitrack, filters and reliable export.
- Smooth 4K thanks to GPU rendering.
- A simple, clean interface, **compared to Windows Movie Maker**, but with advanced features
  (color correction, chroma key) and a rearrangeable workspace.

### What people dislike
- An interface considered **dated**; advanced effects are hard to achieve.
- Few effects.
- **Preview lag** with large files.
- Linux-specific bugs.

### Implications for OmaMovie
- The comparison with Movie Maker shows that **the mental reference for a "simple editor" is
  still Movie Maker**. There is demand for that positioning on Linux.
- The complaints (slow preview, dated UI) are exactly what OmaMovie targets: a GPU-first
  pipeline and a modern, contextual UI.

---

## 5. Patterns across every editor

What repeatedly shows up as a reason to like (or abandon) an editor:

| Factor | Positive examples | Negative examples | Priority for OmaMovie |
|---|---|---|---|
| **Stability / not losing work** | FCP | Premiere, CapCut, Shotcut | Highest (`CLAUDE.md` §14, §18) |
| **Playback performance** | FCP, Resolve (on strong hardware) | Shotcut with large files, Resolve on weak machines | Highest (GPU-first) |
| **Time to result** | CapCut, iMovie, Descript, Resolve's Cut page | Premiere/Resolve for beginners | High (presets, contextual inspector) |
| **Pricing model** | Resolve Free, FCP one-time purchase, Kdenlive/Shotcut free | Premiere subscription, CapCut paywall | Free project: a natural advantage |
| **Privacy / offline** | Kdenlive, Shotcut | CapCut (terms), Clipchamp (account) | Natural (no cloud) |
| **Captions** | CapCut, Premiere, Resolve | — | High: a caption track in the model |
| **Vertical / social media** | CapCut, Clipchamp | — | Medium: changeable canvas aspect ratio |
| **Modern, clean interface** | CapCut, iMovie, Clipchamp | Shotcut ("dated"), Resolve ("dense") | High |

---

## Sources

- [Fitgap: Final Cut Pro X](https://us.fitgap.com/products/final-cut-pro-x)
- [Temperstack: Final Cut Pro](https://www.temperstack.com/software/final-cut-pro/)
- [Gartner Peer Insights: Final Cut Pro](https://www.gartner.com/reviews/product/final-cut-pro)
- [Unite.AI: Descript review](https://www.unite.ai/descript-review)
- [G2: Descript reviews](https://www.g2.com/products/descript/reviews)
- [Tella: Descript video editing — how does it work](https://www.tella.com/blog/descript-video-editing-how-does-it-work)
- [Geeky Gadgets: Edit videos like a text doc](https://www.geeky-gadgets.com/edit-videos-like-a-text-doc/)
- [Kdenlive: 25.08.1 release](https://kdenlive.org/news/releases/25.08.1/)
- [Planet KDE: State of Kdenlive 2026](https://planet.kde.org/kdenlive-2026-04-18-state-of-kdenlive-2026/)
- [Feedbagel: Kdenlive 2025 development update](https://feedbagel.com/post/kdenlive-2025-development-update-focus-on-stability-performance-and-community-co)
- [Linux Today: Kdenlive 25.08.2 released](https://www.linuxtoday.com/blog/kdenlive-25-08-2-released-with-stability-fixes-and-polished-effects/)
- [Ubunlog: Kdenlive 25.12.2](https://en.ubunlog.com/kdenlive-25-12-2/)
- [AlternativeTo: Kdenlive alternatives](https://www.alternativeto.net/software/kdenlive/)
- [G2: Shotcut reviews](https://www.g2.com/products/Shotcut/reviews)
- [Capterra: Shotcut reviews](https://www.capterra.com/p/173467/Shotcut/reviews/?page=3)
- [VideoHelp: Shotcut reviews](https://videohelp.com/software/Shotcut/reviews)
- [rfp.wiki: Kdenlive vs Shotcut](https://www.rfp.wiki/design-multimedia/media-entertainment/video-editing-software/kdenlive/shotcut)
