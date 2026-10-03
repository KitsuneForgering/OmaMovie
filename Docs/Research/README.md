# Research

Market, UX and technology research that guides OmaMovie. It is not a specification:
recommendations here only become rules once they reach `CLAUDE.md` or an ADR.

**Product and UX**

| Document | Topic |
|---|---|
| [premiere-pro.md](premiere-pro.md) | Why Adobe Premiere Pro is a powerful editor (engine, editing model, color, interop, weaknesses) |
| [imovie.md](imovie.md) | Apple iMovie UI/UX (layout, contextual inspector, magnetic timeline, templates) |
| [movie-maker.md](movie-maker.md) | Windows Movie Maker and its successors UI/UX (storyboard/timeline, Clipchamp) |
| [davinci-resolve.md](davinci-resolve.md) | Why DaVinci Resolve is powerful (pages, Cut page, node color, database projects, OTIO, the Linux situation) |
| [capcut.md](capcut.md) | What people like and dislike about CapCut (templates, captions, speed ramps, paywall, terms of service) |
| [other-editors.md](other-editors.md) | Final Cut Pro, Descript, Kdenlive, Shotcut and patterns shared across editors |

**Technology and platform**

| Document | Topic |
|---|---|
| [hardware-strategy.md](hardware-strategy.md) | **Synthesis**: role of each API, single Vulkan device, runtime selection, proposed `CLAUDE.md` changes, spikes |
| [vulkan.md](vulkan.md) | Vulkan-Hpp, Vulkan Video, VA-API→Vulkan, FFmpeg `hwcontext_vulkan`, Qt Quick on an external device, libplacebo |
| [cuda.md](cuda.md) | NVDEC/NVENC without the toolkit, CUDA↔Vulkan interop, kernels through PTX (clang), hybrid laptops |
| [opencl.md](opencl.md) | The analysis that led to **removing OpenCL** (rusticl, NEO, ROCm, Vulkan interop, the Blender lesson) |
| [omarchy-integration.md](omarchy-integration.md) | Theme, window opacity, screen recordings, Quickshell shell, menu, installed drivers, distribution |

## Method

- Sources: official documentation (Adobe Help, Apple Support, Blackmagic, Kdenlive), Wikipedia,
  trade press, independent benchmarks (Puget Systems) and review aggregators (G2, Capterra;
  treated as a signal of opinion, not as measurement). Links in each document.
- Claims without a specific source are marked **(general knowledge)** or **(general observation)**.
- Each section ends with **Implications for OmaMovie**, linking the finding to `CLAUDE.md` sections.
- Goal: extract principles. Copying visual identity or proprietary behavior is forbidden (`CLAUDE.md` §11).

---

## Synthesis

### Where each product sits

```
simple ◄───────────────────────────────────────────────────────────────► powerful
Movie Maker/Photos   iMovie    CapCut/Clipchamp   Shotcut/Kdenlive   FCP   Premiere/Resolve
(storyboard)    (FCP X engine) (presets, social)   (free, MLT)            (cockpit/pages)

OmaMovie: a surface close to iMovie/CapCut, an engine close to Premiere/Resolve.
On Linux, the space between Kdenlive/Shotcut and Resolve is empty.
```

### Conclusions

1. **Simplicity comes from contextual disclosure, not from removing features.** iMovie's
   adjustments bar and Clipchamp's properties panel show only what applies to the selection.
   Premiere solves it with 16 workspaces. OmaMovie's contextual inspector is the right path.
   Also adopt the **visual indicator of which adjustments are active** on a clip (iMovie).

2. **A redesign that removes capability causes rejection.** iMovie '08 and Windows Live Movie
   Maker had to bring features back in the following versions. This confirms "hide, do not
   remove" and "grow without rewriting the core".

3. **A professional engine under a simple UI works.** iMovie has used the Final Cut Pro X
   engine since 2013 and exports to it. That is OmaMovie's thesis, with the difference of
   being a single app: progressive disclosure needs more care.

4. **Named operations over a generic compositor.** iMovie's PiP, cutaway, split screen and
   green screen are presets. In OmaMovie they must produce normal layers and parameters in the
   render graph, editable afterwards, without iMovie's layer ceiling.

5. **Semantic storyboard ↔ timeline zoom** (Movie Maker 2012). A single TimelineView whose
   minimum zoom becomes a storyboard (equal-size clips, focus on order). A strong UX
   differentiator. Requires virtualization and a thumbnail cache.

6. **"Powerful" starts in the engine**: real-time GPU playback of native media, hardware
   decode/encode with a fallback, a float pipeline, proxies. Premiere also shows that
   **effects without a GPU path need a visible indication** (the render bar).

7. **A complete editing model in the core, a gradual UI.** Ripple/roll/slip/slide, split edits
   and three-point editing should exist as timeline commands early. iMovie shows roll and
   split edits can have a friendly UI (Precision Editor, Clip Trimmer). Premiere shows full
   keyboard access.

8. **Automatic color management** (Premiere 25.2: color space detection, presets, effects
   that know their color space). Feeds ADR-0006.

9. **Stability is part of power.** The main criticism of Premiere is instability and corrupted
   projects. Atomic save, tested migrations and parser fuzzing are a competitive advantage,
   not bureaucracy.

10. **`.prproj` is gzip-compressed XML.** A relatively accessible import target to validate
    `Importer → ProjectIR`, after OTIO/FCPXML/EDL.

11. **Resolve on Linux leaves an opening.** The free version does not decode H.264/H.265 or
    AAC on Linux, official support is NVIDIA only and the documented audio path is ALSA. Phone
    video + an AMD/Intel GPU + PipeWire, the typical Omarchy setup, is poorly served. This is
    OmaMovie's **clearest positioning**.

12. **Simple and fast are not opposites.** Blackmagic created the Cut page (source tape, dual
    timeline) for professionals on a deadline. CapCut wins on time to result. OmaMovie's
    simplicity must also serve advanced users.

13. **Overview + detail in the timeline**: Resolve's dual timeline and Movie Maker's semantic
    zoom attack the same problem. Prototype both approaches (or a minimap) in the TimelineView.

14. **Graph inside, stack outside.** Resolve 21 added a layer-list view to its color node
    graph. OmaMovie's render graph is a DAG; the UI shows the stack.

15. **Projects as files, not in a database.** Resolve projects live in a database
    (local/PostgreSQL/cloud) and need a `.drp` export to become a file. This confirms
    `CLAUDE.md` §14/§17: the file is the source of truth.

16. **OTIO is the first interop target.** Resolve has exported OTIO natively since 18.5: a
    Resolve → OmaMovie path without reverse engineering.

17. **Outcome operations, not mechanism operations.** CapCut sells "hero time", not "speed
    keyframes". Intent presets first, parameters later.

18. **Competitors' weak points are OmaMovie's differentiators**: paywall and content terms
    (CapCut), subscription (Premiere), crashes with lost work (Premiere, CapCut, Shotcut), slow
    preview (Shotcut), learning curve and demanding hardware (Resolve). Offline, no account,
    stable and GPU-first.

### Architecture requirements that emerged

| Requirement | Origin | Affects |
|---|---|---|
| Fast seek and a multi-density thumbnail cache with aggressive cancellation | Skimming (iMovie), semantic zoom (Movie Maker) | `media`, cache, job system |
| Interactive preview overlays that emit commands | Crop/position/eyedropper in the viewer (iMovie) | PreviewPanel, `timeline` |
| Magnetism as a UI editing policy, not a model restriction | Magnetic timeline (iMovie/FCP X) | `timeline` |
| GPU capability declared per effect and indication of non-real-time sections | Mercury (Premiere) | `compositor`, UI |
| Templates produce a normal, editable timeline | Trailers (iMovie), AutoMovie | `timeline`, `project-ir` |
| Per-stream color metadata and color-space-aware effects | Color management (Premiere) | `media`, `compositor` |
| Editing during playback (dynamic trim) | Premiere | Threading (ADR-0003) |
| **Curve-based time mapping** (not only constant speed), decided before implementing speed | Speed ramps (CapCut) | `base` time, `timeline` (ADR-0002) |
| A first-class caption/text track, SRT/VTT import/export | CapCut, Premiere, Resolve | `timeline`, `project` |
| A canvas with changeable aspect ratio; decide normalized coordinates vs. pixels | Vertical/horizontal (CapCut, Clipchamp) | `compositor`, `timeline` |
| Markers in the timeline model | Beat sync (CapCut), review | `timeline` |
| Serializable commands with no UI dependency (base for future scripting/MCP) | Scripting and MCP (Resolve 21.1) | `timeline` |
| An action system independent of the input device | Speed Editor (Resolve) | UI |
| Background render filling the cache when real time is not possible | Final Cut Pro | `compositor`, cache, job system |
| Transcript as a view of the timeline (words with a `TimeRange`) | Descript, Premiere | `timeline` (future) |
| Benchmarks including Intel/AMD integrated GPUs | Resolve needs strong hardware | `tools/bench` |

### Suggested next research

- **OpenTimelineIO**: data model as a reference for ProjectIR (priority, given Resolve's native support).
- **Final Cut Pro in depth**: storylines, roles, FCPXML (interop target).
- **Kdenlive/MLT internals**: MLT architecture, why 10-bit and GPU decode arrived late, project format (importer candidate).
- **Linux hardware pipeline**: current state of VA-API, Vulkan Video and DMA-BUF interop on Mesa/NVIDIA (technical base for `CLAUDE.md` §7.3).
- **Avid Media Composer and Vegas**: editing model and formats (future interop targets).
