# Post-production effects: core set and third-party catalog

> Documentary review, 2026-10-04. This proposes product priorities and a future
> extension path; it is not an accepted ADR or a working marketplace.

## Conclusion and question

For OmaMovie's short Omarchy recordings and phone videos, the next core visual
operation worth piloting is a **region mask**: opaque cover for hiding an area,
with optional local blur or highlight. A **focus/zoom preset** should reuse the
transform keys already present. A simple **chroma key** may follow the planned
PiP overlay. Stabilization, video denoise and automatic tracking need footage
and performance pilots first. A marketplace can start as a curated catalog of
editable presets; executable third-party effects require a separate host contract
and compatibility gate. These are priorities to test, not measured user demand.

The baseline already has color, CDL/curves/LUTs, filter looks, blur/sharpen,
vignette, crop, transform keys, transitions, audio EQ, FFT noise reduction and
peak normalization ([plan](../implementation-plan.md)). Compare candidates on
task completion, output correctness, undo/save/export, preview cost, trust and
the simpler manual alternative. Hypotheses: selective effects help recordings
more than additional whole-frame looks; editable presets reduce effort; only
some tasks justify third-party code. A representative task study showing the
opposite would change the ranking.

## Ideas from the editors already researched

| Source in this directory | Keep in OmaMovie's core or pilot | Leave to catalog / omit for now |
|---|---|---|
| [iMovie](imovie.md), [Final Cut](final-cut-pro.md) | Contextual controls and editable named operations; dependent overlays must remain attached through edits | Effect branding, trailer themes and advanced motion design have no demonstrated task benefit here |
| [Movie Maker / Clipchamp](movie-maker.md) | Selected-clip properties and a simple library/viewer/timeline workflow are useful places to expose effects | A separate storyboard effect mode has no demonstrated advantage over the current timeline/minimap |
| [CapCut](capcut.md) | Editable timed captions and speed curves already belong to M8; presets should produce normal edit parameters | Automatic transcription and a large template library need separate evidence |
| [Premiere](premiere-pro.md), [Resolve](davinci-resolve.md) | Color input/working/output discipline and measured scopes; local masks can reuse existing compositor stages | Full grading/compositing suites and unrestricted plugin hosting would outrun the simple-editor goal |
| [Kdenlive/Shotcut](other-editors.md) | They are task and performance baselines, not proof of a missing market | Their effect counts or GPU labels alone should not set priorities |

The editor research mainly documents vendor behavior. It contains no comparative
effect-use frequency or representative preference study. New direct sources below
establish mechanisms, not demand.

**What to remove from the built-in picker:** audit the overlapping Vintage,
Sepia, Cool and Warm looks against saved color/LUT presets in an M6 task study.
If a look is redundant and seldom chosen, remove it from new-project discovery;
retain its renderer and stable ID for old projects until a tested migration exists.
Keep Black & White and Vignette discoverable while they solve distinct one-step
tasks. This is a proposal to test, not a deletion decision based on taste.

## Candidate decisions and falsifying checks

| Candidate | Task and simpler baseline | Proposed gate, not run |
|---|---|---|
| Rectangle/ellipse mask; opaque cover, local blur/highlight | Hide a notification or emphasize a UI region; crop or cut may suffice | Start static, then manually keyed position. Check every frame of a decoded export, save/load, undo/redo and 1080p60 preview. Any uncovered sensitive frame rejects a privacy claim. |
| Focus/zoom preset | Point to a small control; manual transform keys already exist | Emit editable keys; compare task steps, time and encoded text readability with the manual baseline. |
| Chroma key | Key a webcam over a recording; rectangular PiP may suffice | After PiP/alpha support, compare edges/spill and color on real green-screen footage and cost in preview/export. |
| Loudness-aware full-mix export | Match narration/music better than clip peak normalize/manual gain | Choose target and true-peak ceiling for an intended destination, then measure the encoded mix with an independent meter. |
| Stabilization and video denoise | Rescue shaky/noisy phone video; simpler baseline is selective crop/color or leaving footage intact | Compare matched footage for jitter, borders, lost detail, flicker, p95/p99 preview time and export time before committing. |
| Automatic tracking | Follow a moving face or region; manually keyed mask is baseline | Accept only if correction rate and total task time improve, with export inspected frame by frame. |

[Adobe's mask guide](https://helpx.adobe.com/in/premiere/desktop/add-video-effects/work-with-masks/create-masks-using-shapes.html)
documents assigning shape masks to blur, color and opacity. [FFmpeg's
`feedback` examples](https://ffmpeg.org/ffmpeg-filters.html#feedback) apply blur,
opaque fill or pixelation to a rectangle. An opaque cover removes selected
pixels from the rendered image; blur and pixelation leave information. A
[2025 preprint](https://arxiv.org/abs/2512.16086) reports reversal attacks
against some practical Gaussian-blur implementations on still images. Its
results do not validate a video redaction workflow: mask motion, export and
audio still require inspection. [Premiere's tracking guide](https://helpx.adobe.com/premiere/desktop/add-video-effects/work-with-masks/track-masks.html)
also calls for reviewing tracked frames and notes high-contrast subjects work
best. [Kdenlive's tracker manual](https://docs.kdenlive.org/en/effects_and_filters/video_effects/alpha_mask_keying/motion_tracker.html)
shows tracked positions can become keyframes.

[FFmpeg `colorkey`](https://ffmpeg.org/ffmpeg-filters.html#colorkey) is a
documented simple RGB key. [`vidstabdetect`/`vidstabtransform`](https://ffmpeg.org/ffmpeg-filters.html#vidstabdetect)
are two-pass and require a libvidstab-enabled build; [`deshake`](https://ffmpeg.org/ffmpeg-filters.html#deshake)
targets small shifts. [`bm3d`](https://ffmpeg.org/ffmpeg-filters.html#bm3d)
provides a video-denoise comparator, with source-dependent strength. None of
these filter descriptions proves integration with OmaMovie's Vulkan preview.
[ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en)
defines loudness/true-peak measurement and [FFmpeg `loudnorm`](https://ffmpeg.org/ffmpeg-filters.html#loudnorm)
documents processing options; neither chooses OmaMovie's delivery target.

## Marketplace: staged design hypothesis

**Stage 1 — curated preset catalog.** Third parties submit a versioned manifest,
license, preview, description and parameter values built only from OmaMovie's
existing effects/commands. The app shows author, version and contents before a
user installs a pack. Install per user, explicitly; keep versions and hashes so
a saved project can identify its dependencies. A preset creates editable native
parameters and remains usable if the pack later disappears. A repository of
static manifests and downloadable assets is enough for a pilot; no account,
payment service or code loader is needed to discover whether sharing is useful.
For recording-time presets, generate an ordinary edit on imported footage first;
integration with the Omarchy recorder needs its own lifecycle and permission
contract. The catalog may carry both production-style presets and post-production
looks, but must say when each is applied.

**Stage 2 — executable effects, only after a host pilot.** A third-party effect
must have a stable ID/version, declared inputs/outputs, color/alpha/time domain,
parameters and defaults, deterministic save/undo/export semantics, supported
platform/GPU paths, resource limits and a missing-effect behavior. Test one
out-of-process CPU effect on representative footage, with timeout/crash
recovery and a visible bypass/error state, before specifying a public SDK.
Do not load downloaded native libraries into the editor process by default:
they can crash it or access its data. A process boundary costs frame transfers;
measure that cost before claiming real-time preview. Any GPU handoff needs a
separate Vulkan ownership/synchronization proof. The catalog must distinguish
presets from executable code and disclose the latter's author, source, permissions,
update path and review status. Review is a distribution policy, not a security
guarantee. Failed updates must leave a usable previous version; projects pin an
effect identity/version and report missing or incompatible effects before export.

**Alternative considered:** [OpenFX 1.5.1](https://openfx.readthedocs.io/en/main/Reference/ofxImageEffectAPI.html)
is an established image-effect API with parameters, clips and host suites.
Its [loading model](https://openfx.readthedocs.io/en/main/Reference/ofxCoreAPI.html)
loads a binary into the host, and its [rendering model](https://openfx.readthedocs.io/en/main/Reference/ofxRendering.html)
documents CPU, OpenGL and other GPU paths; the reviewed API does not provide
OmaMovie's current Vulkan image contract or process isolation. Adopting a full
OFX host solely to launch a marketplace is therefore unjustified now. Revisit
with an actual third-party effect and a measured CPU-transfer/host prototype.
[Blender Extensions](https://extensions.blender.org/about/) demonstrates an
online catalog with install confirmation and submission review, but its product
policy and implementation are not evidence that the same marketplace will work
for OmaMovie.

## Verification and source record

This was **documentary analysis**, not an implemented or benchmarked effect or
marketplace. The proposed tests above are pending. Research budget: the seven
existing editor notes, local plan/ADR-0012, primary effect/API documentation
and one relevant privacy preprint; stopped when the candidate order and decisive
pilots were clear. The largest uncertainty is whether target users need these
tasks often enough to justify an extension ecosystem.

Accessed 2026-10-04 as extracted HTML: Adobe, *Create masks* (updated
2026-03-17), shape-mask steps; Adobe, *Track masks* (updated 2026-03-09),
limitations/review steps; Kdenlive, *Motion Tracker* (26.08 manual), keyframes
and options; FFmpeg, *Filters Documentation* (live revision unknown), sections
named above; ITU-R, *BS.1770-5* (2023-11), title/status/scope only, not the full
recommendation; Mahloujifar et al., *Privacy Blur* (preprint 2025-12-18),
abstract/introduction; Academy Software Foundation, *OpenFX 1.5.1*, Image
Effect/Core/Rendering sections; Blender Foundation, *Extensions Platform* (live
date unknown), catalog/install/review description. Vendor documents share their
author's evidential lineage; they establish documented behavior, not comparative
quality or adoption. Local docs were read from a working tree with unrelated
uncommitted changes.
