## Proposed gate, before optimizations

Use a reproducible generated set plus real Omarchy/phone footage. Declare codec, GOP,
resolution, storage, driver, FFmpeg/Qt builds and project edit revision. Compare: (A) one
two-hour source with a few cuts, (B) two hours assembled from 5,000 short clips across
video/audio tracks, (C) a two-hour 4K long-GOP source with repeated random seeks, and
(D) a 10-minute section with costly effects. The 10-minute fixture is the control.
The 5,000-clip fixture is **synthetic stress**, not a claim about typical users.

1. Record cold/warm open, save and reopen time; peak RAM; project bytes; parser, DTO,
   validation, media reimport and UI-model phases separately. Require an exact project
   round trip and explicit failure when the format cap is exceeded. No parser/store change
   follows merely from total open time.
2. On the reference Intel machine, record p50/p95/p99 and maximum for split, trim,
   undo, timeline pan/zoom, selecting a clip, play start and random seek; track UI-thread
   stalls, decoder restarts, dropped frames, CPU/GPU/RAM and cache disk use. **Proposed
   interaction target:** p95 under 100 ms and no single routine edit stall above 250 ms
   in the 5,000-clip scenario. Revisit a threshold only as an explicit product decision,
   not after seeing a favorable run. Profile the worst path before adding an index.
3. Compare identical source/output settings for current viewer, proxy and/or preview
   render as each becomes available. Include generation time and disk budget. Accept a
   cache only if it improves the affected p95/p99 or drops without changing result,
   and deletion/failure of cached data leaves the original project usable.
4. Once export exists, run the full two-hour timeline with sound, effects and seeks.
   Check the output with an independent decode, sampled content at cuts/transition
   boundaries, audio continuity/sync at beginning/middle/end, cancellation and disk-full
   recovery. A complete file plus `ffprobe` metadata alone is insufficient.
5. Force a crash or power-loss surrogate around save and autosave on the supported local
   filesystem; verify recovery of the last acknowledged save and the documented autosave
   state. Test missing/moved media and LUTs, relink, and a failed proxy cache.

**Stop conditions:** no release claim for a workload that loses acknowledged edits,
silently changes output, uses unbounded memory/disk, or cannot produce a verified movie.
For claims of smooth playback, apply M4's frame-time/A/V criteria to the declared scene.

[Return to long-form-editing.md](long-form-editing.md)
