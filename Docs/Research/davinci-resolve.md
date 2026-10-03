# DaVinci Resolve: why it is a powerful editor

> Research done on 2026-10-02 (current version: Resolve 21.x). Sourced facts are linked in the
> [Sources](#sources) section. Items marked **(general knowledge)** are widely documented but
> have no specific source in this research; verify before using them as requirements.

---

## 1. Summary

Resolve is powerful for five reasons:

1. **Cinema-grade color.** It was born as a color correction system, and its node-based grading
   is considered the industry standard.
2. **Everything in one program.** Editing, compositing (Fusion), color, audio (Fairlight) and
   delivery as "pages" of the same app, on the same project.
3. **Two editing philosophies.** The **Cut** page is fast and lean; the **Edit** page is the full traditional NLE.
4. **Pricing model.** A very capable free version, and Studio as a **one-time purchase** (about US$ 299).
5. **It runs natively on Linux**, which none of the big competitors do.

On the other hand: a steep learning curve, demanding hardware, and **on Linux the free version
does not decode H.264/H.265 or AAC**. That last point is very relevant for OmaMovie.

---

## 2. History

**(general knowledge, unless stated)**

| Year | Milestone |
|---|---|
| 1980s–2000s | da Vinci Systems builds color correction systems for telecine |
| 2009 | Blackmagic Design buys da Vinci |
| ~2011 | Free (Lite) version released |
| 2014 (v11) | Becomes a full editor (Edit page) |
| 2017 (v14) | Fairlight (audio) integrated |
| 2018 (v15) | Fusion (node compositing) integrated |
| 2019 (v16) | **Cut** page |
| 2020-11 | **Speed Editor** (dedicated editing keyboard) |
| v18 | "Databases" renamed "Project Libraries"; Blackmagic Cloud |
| v18.5 | Native **OpenTimelineIO** support |
| 2025-05 (v20) | 100+ new features, including AI tools |
| 2026-06 (v21) | **Photo** page, more AI tools |
| v21.1 | **MCP** support and 20 new scripting APIs |

---

## 3. Pages: one app, several contexts

| Page | Function |
|---|---|
| Media | Import and organization |
| **Cut** | Fast assembly |
| **Edit** | Full traditional editing |
| **Fusion** | Node-based compositing and VFX |
| **Color** | Node-based grading |
| **Fairlight** | Audio mixing and mastering |
| Deliver | Export |
| Photo (v21) | Photo editing with Resolve's color tools |

Every page works on the same timeline and project. There is no export/import between apps,
unlike Premiere + After Effects + Audition, and that is cited as a reason for switching (see
`premiere-pro.md` §10).

### Implications for OmaMovie

- Pages are a third way to organize complexity, alongside Premiere's workspaces and iMovie's
  contextual inspector. They work for professional workflows in phases (assemble → grade →
  mix), but mean context switches and a dense interface on every page.
- For OmaMovie the contextual inspector stays the main model. A future "focus mode" (e.g.
  full-screen color) may exist, but as an extension of the inspector, not an app inside the app.
- The strongest architectural lesson: **a single project model for every function**. Color,
  audio and compositing read and write the same model. OmaMovie already follows this
  (`timeline` → render graph); do not create parallel models per function.

---

## 4. The Cut page: speed for professionals

Introduced in v16 for fast assemblies on short deadlines:

- **Source tape**: one button shows every clip in the bin in the viewer as **a single
  continuous tape**. You can scrub through all the material and edit without hunting clip by clip.
- **Dual timeline**: the upper timeline shows **the whole program**; the lower one shows **the
  working region** zoomed in. Both are editable. Blackmagic justifies it by saying that zooming
  and scrolling the timeline is slow.
- A lean interface with fewer visible tools.
- Designed for the **Speed Editor** (§9).

### Implications for OmaMovie

- Blackmagic itself, owner of the most "complete" editor, created a simplified page. This
  confirms **simple and fast are not opposites**: the Cut page is simple and made for
  professionals on a deadline.
- The **dual timeline** is an alternative to Movie Maker's semantic zoom (`movie-maker.md` §4).
  Both solve the same problem (overview + detail without constant zooming). Evaluate in the
  TimelineView prototype: a compact overview bar (minimap-style) above the timeline may give
  the benefit without duplicating the interface.
- **Source tape** is a way to browse the library with the same skimming engine (`imovie.md` §6).
  Same technical requirement: fast seek and a thumbnail cache.

---

## 5. Color

- **Node-based grading**: each node is one correction step; nodes are connected, reordered and
  changed without affecting the rest of the grade. Non-destructive by construction.
- Tools: color wheels, curves, **qualifiers** (selection by color/luminance), **power windows** (tracked masks).
- v21: viewing nodes as a **layer list** (layer-list node graph), up to 8-layer stacks per node,
  improved ACES workflows, group grading with versions.
- Independent HDR trims for Dolby Vision, HDR10+ and HDR Vivid (21.1).

### Implications for OmaMovie

- **Resolve v21 added a layer-list view for the node graph.** Even for colorists, a pure graph
  is not always the best interface. For OmaMovie: **render graph as a DAG internally, a list of
  layers/effects in the UI**. The graph stays in the core; the UI shows the stack.
- Full node-based grading is an initial non-goal. But the effect interface should allow a color
  effect to have several internal stages, so that door stays open.
- ACES and HDR reinforce ADR-0006 (working color space), in line with Premiere 25.2.

---

## 6. Fusion and Fairlight

- **Fusion**: node-based compositing integrated as a page. v20 brought more advanced multi-layer
  compositing; v21/21.1 added the Krokodove toolset (70+ graphics, 3D and procedural tools).
- **Fairlight**: a complete audio workstation inside the editor; v20 brought IntelliCut (removes
  silence and splits dialogue between speakers); v21 brought folder tracks.

### Implications for OmaMovie

- After Effects/Fusion-style compositing is a **non-goal**. Fusion shows the category's ceiling,
  not a requirement.
- Audio: silence removal is a concrete, useful function for spoken content (podcasts,
  tutorials). It can be done with audio level analysis, without AI. A low-cost future candidate.

---

## 7. Projects: a database, not a file

- Projects live in a **Project Library** (formerly "Database"):
  - local: a structure managed by Resolve on disk;
  - network: **PostgreSQL** (through the "DaVinci Resolve Project Server", a bundled PostgreSQL);
  - **Blackmagic Cloud**: hosted libraries for collaboration.
- To get a portable file you must **export a `.drp`**, which contains timelines, edits, grades
  and settings, **without media**. At the destination Resolve relinks.
- Forums report crashes and failures when copying libraries to PostgreSQL.

### Implications for OmaMovie

- The database model is great for studio collaboration but adds friction for individual users:
  the project is not a file you can copy, version or send.
- **It reinforces the `CLAUDE.md` §14/§17 decision**: the project file is the source of truth;
  SQLite only for indexes/cache/metadata. An OmaMovie project should be copyable, versionable
  in git and attachable to an email.
- `.drp` is a potential import target. Investigate the format before prioritizing it (`Docs/formats/`).

---

## 8. Interoperability

- **Native OpenTimelineIO since v18.5**: import/export of `.otio` (timeline metadata only) and
  `.otioz` (timeline plus media).
- XML, AAF and EDL **(general knowledge)**.

### Implications for OmaMovie

- **OTIO is the first interop target.** Resolve (and others) export OTIO natively. That enables
  a Resolve → OmaMovie path **without reverse engineering**, validating `Importer → ProjectIR`
  with an open, documented format.
- `.otioz` (timeline + media) is a good model for an eventual "export packaged project" in OmaMovie.

---

## 9. Dedicated hardware: Speed Editor

- An editing keyboard made for the Cut page (released in November 2020).
- A heavy metal **search dial** to move through the timeline quickly; with a trim button held,
  the dial becomes a **real-time trim** control.
- Shuttle/jog/scroll buttons change the dial's mode; one key per editing function.
- USB-C or Bluetooth.

### Implications for OmaMovie

- Shows the value of mouse-free editing: the same principle as J/K/L in `CLAUDE.md` §11.2.
- The central action system must be **independent of the input device**: keyboard today,
  jog/shuttle controllers (USB HID/MIDI) in the future, mapped to the same actions. Do not
  implement it now; just do not couple actions to keyboard events.

---

## 10. Scripting, AI and MCP

- Scripting API (Python/Lua) **(general knowledge)**. 21.1 added 20 APIs (render presets, media
  pool, multicam, timeline properties, audio normalization) and **dropped Python 2**.
- **MCP (21.1)**: assistants such as Claude and ChatGPT Codex drive Resolve in natural language
  (analyze projects, organize media, change settings, batch render, build highlight reels).
- v20 AI: IntelliScript (timeline from a script), animated captions, Multicam SmartSwitch
  (angle switching by who is speaking), Magic Mask, depth map.
- v21 AI: IntelliSearch, CineFocus, facial refinement.

### Implications for OmaMovie

- The timeline command system (`CLAUDE.md` §10: every edit is a command) is the natural base for
  scripting and automation. A well-defined command serves the UI, the CLI, scripts and an
  eventual MCP server.
- **Do not implement scripting now.** Just keep commands serializable and free of UI
  dependencies, which undo/redo and tests already require.

---

## 11. Resolve on Linux

Highly relevant, because it is the professional editor an Omarchy user would use today.

| Aspect | Situation |
|---|---|
| Official distro | A **Rocky Linux 8.6**–based environment; Ubuntu/Mint work in practice |
| GPU | **NVIDIA with proprietary drivers** is the only officially supported path (CUDA ≥ 12.8, ≥ 4 GB VRAM). AMD with AMDGPU Pro works, but CUDA-only effects are slow. Intel is not mentioned |
| H.264/H.265 (Free) | **Decode and encode not supported** (licensing) |
| H.264/H.265 (Studio) | Decode supported; **encode only on NVIDIA** |
| AAC | **Not supported on Linux**, not even in Studio |
| Audio | ALSA documented; PipeWire not mentioned |
| Common workaround | Transcode with FFmpeg to DNxHR and remux audio to PCM before importing |

### Implications for OmaMovie

**This is the biggest opportunity found in the research.** On Omarchy the typical user has phone
video (H.264/HEVC + AAC), often an AMD or Intel GPU, and PipeWire. The free Resolve cannot open
that material without transcoding, and support outside NVIDIA is partial.

OmaMovie can take exactly that space:
- H.264/HEVC/AV1 + AAC through FFmpeg and VA-API, **without transcoding**;
- **Intel and AMD as first-class citizens** (VA-API/Vulkan Video), not only NVIDIA;
- native PipeWire;
- packaging for Arch instead of "supported on Rocky Linux".

This reinforces the priorities in `CLAUDE.md` §7.3. Hardware tests must cover AMD and Intel, not only NVIDIA.

---

## 12. Weaknesses and criticism

- **A steep learning curve** and a dense interface; professional defaults scare beginners.
- **Demanding hardware**; frequent complaints about performance and crashes on modest machines.
- Free version limits on resolution, effects, codecs and GPU use.
- Linux: codec and GPU limitations (§11).

### Implications for OmaMovie

- Resolve shows the capability ceiling but does not serve people who want to edit quickly on a
  laptop. OmaMovie must **work well on modest hardware** (Intel/AMD integrated GPUs), with
  proxies and explicit degradation instead of freezing.
- Benchmarks (`CLAUDE.md` §23) must include at least one machine with an integrated GPU.

---

## 13. Prioritization table for OmaMovie

| Resolve capability | Adopt? | When | Module |
|---|---|---|---|
| A single project model for every function | Yes (already planned) | From the start | `timeline`, `project` |
| Overview + detail timeline (dual timeline / minimap) | Evaluate in the prototype | Phase 10 | UI |
| Source tape (library as a continuous tape) | Evaluate | After the library | UI, `media` |
| Internal DAG render graph, layer-stack UI | Yes | Phase 5 | `compositor` |
| OTIO import | Yes, first interop target | Phase 12 | `importers` |
| Projects in a database | **No**; the file is the source of truth | — | `project` |
| Silence removal | Future, without AI | After audio | `audio` |
| Actions independent of the input device | Yes (design) | Phase 10 | UI |
| Serializable commands (base for scripting/MCP) | Yes (design) | Phase 9 | `timeline` |
| Scripting/MCP | Not now | Future | — |
| Full Fusion/Fairlight | No | Non-goal | — |
| Consumer codecs + AMD/Intel + PipeWire on Linux | **Yes, a differentiator** | Phases 2–8 | `media`, `audio` |
| `.drp` import | Investigate | Future | `importers` |

---

## Sources

- [Blackmagic Design: DaVinci Resolve 20 announcement](https://www.blackmagicdesign.com/media/partial/release/20250404-02)
- [Post Magazine: Blackmagic Design unveils DaVinci Resolve 20](https://www.postmagazine.com/Press-Center/Daily-News/2025/Blackmagic-Design-unveils-DaVinci-Resolve-20-wit.aspx)
- [Post Magazine: Blackmagic Design releases v20 of DaVinci Resolve & Fusion](https://www.postmagazine.com/Press-Center/Daily-News/2025/Blackmagic-Design-releases-V-20-of-DaVinci-Resol.aspx)
- [Newsshooter: DaVinci Resolve 20 announced](https://www.newsshooter.com/2025/04/04/blackmagic-design-davinci-resolve-20-announced-with-100-new-features-including-ai-enhancements/)
- [Broadcast Now: NAB 2025 — AI rough cut and audio generation in Resolve](https://www.broadcastnow.co.uk/production-and-post/nab-2025-blackmagic-adds-ai-powered-rough-cut-and-audio-generation-to-resolve/5203915.article)
- [PetaPixel: DaVinci Resolve 21 officially released](https://petapixel.com/2026/06/03/davinci-resolve-21-officially-released-with-new-photo-editing-ai-tools/)
- [CineD: DaVinci Resolve 21 announced](https://www.cined.com/davinci-resolve-21-announced-new-photo-page-eight-new-ai-tools-tethered-camera-controls-and-more/)
- [Motion Media: DaVinci Resolve 21.1 update](https://www.motionmedia.com/mm-blog/davinci-resolve-21-1-update-new-features)
- [RedShark News: DaVinci Resolve 21.1 new features](https://www.redsharknews.com/davinci-resolve-21.1-new-features-release)
- [Storyblocks: Color grading in DaVinci Resolve](https://www.storyblocks.com/resources/tutorials/davinci-resolve-color-grading)
- [TourBox: A complete guide to nodes in DaVinci Resolve](https://www.tourboxtech.com/en/news/davinci-resolve-nodes.html)
- [Pro Video Coalition: DaVinci Resolve 16](https://www.provideocoalition.com/davinci-resolve-16-adds-lufs-audio-loudness-standards-linear-features/)
- [Wipster: Inside DaVinci Resolve's new Cut page](https://wipster.io/blog/inside-davinci-resolves-new-cut-page)
- [PremiumBeat: DaVinci Resolve 16 Cut page review](https://www.premiumbeat.com/blog/davinci-resolve-16-cut-page-review/)
- [Uncle: DaVinci Resolve Project Library vs database](https://tryuncle.com/learn/davinci-resolve/davinci-resolve-project-library-vs-database)
- [Blackmagic Forum: Copy DB from local to Postgres fails or crashes](https://forum.blackmagicdesign.com/viewtopic.php?p=746517)
- [Prism Pipeline: OpenTimelineIO integration](https://prism-pipeline.com/docs/latest/general/miscellaneous/opentimelineio/)
- [Digital Production: DaVinci Resolve Speed Editor release](https://digitalproduction.com/2020/11/11/davinci-resolve-speed-editor-release/)
- [tal.org: Blackmagic DaVinci Resolve on Linux](https://www.tal.org/tutorials/blackmagic-davinci-resolve-linux)
- [NixOS Wiki: DaVinci Resolve](https://wiki.nixos.org/wiki/DaVinci%20Resolve)
- [Blackmagic Forum: Can't play clips, use CUDA, .mp4 or h.264 in Linux](https://forum.blackmagicdesign.com/viewtopic.php?p=742208)
- [Movavi: How to use DaVinci Resolve (beginner's guide)](https://www.movavi.com/support/how-to/how-to-use-davinci-resolve/)
- [rfp.wiki: DaVinci Resolve vs Final Cut Pro](https://www.rfp.wiki/vendors/davinci-resolve/final-cut-pro)
