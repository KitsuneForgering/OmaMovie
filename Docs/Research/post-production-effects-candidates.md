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

[Return to post-production-effects.md](post-production-effects.md)
