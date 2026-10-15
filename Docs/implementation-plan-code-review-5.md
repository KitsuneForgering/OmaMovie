### Item-by-item code review, part 5 (2026-10-14)

Continues [part 4](implementation-plan-code-review-4.md). Base revision `0e12ece`.

#### 11. `apps/omamovie`: save, autosave and export

Save and autosave are sound: the document is copied on the UI thread and written on the
job worker through `project::save` (atomic, §14); an autosave finishing after a save or
discard deletes its own file instead of resurrecting it.

- [x] Overlap rendering and encoding in export. The export loop renders and reads back
  frame n, then encodes it, then starts frame n+1 (`apps/omamovie/src/exporter.cpp:131`),
  so per-frame cost is the sum of the stages: 8.3 ms render + readback and 7.4 ms CPU
  RGBA→NV12, upload and VA-API
  encode at 1080p (export audit, 2026-10-14). On the same machine and file, MLT's melt
  (Kdenlive's engine) exported in 6.5 s against OmaMovie's 9.9 s
  ([evidence](Research/evidence/2026-10-14-cross-editor-export.txt)). With one frame
  rendered ahead (a bounded queue of one or two frames, the encoder on the export job and
  the render on its own job) the cost approaches the slower stage: derived ~8 ms instead
  of ~16 ms per frame, not measured. Keep frame order, cancellation and the device-loss
  fallback; check with the cross-editor run and the export frame tests. *Done
  (2026-10-14)*: `EncodeStage` (`export_encoder.cpp`) owns the writer on one worker of its
  own pool, fed by a two-frame `BoundedQueue` (new blocking `pop`, tested under TSan); the
  export tests pass, also under TSan. Cuts scene, 3 runs at load 8-12: VA-API 5.9-6.0 s
  (6.7 s median before) with byte-identical output; x264 12.6-14.5 s (12.4 s before): the
  encoder is the slower stage (12-13 ms VA-API, 32-38 ms x264 per frame under load), so
  overlap hides only the render. Rerun the five-round comparison on an idle machine.
- [x] Then reduce the readback itself: convert to NV12 on the GPU before reading back
  (1.5 bytes per pixel instead of 4, no CPU RGBA→NV12 pass), and only after that consider
  handing a Vulkan image to VA-API as a DMA-BUF (zero copy, §7.2), which needs the
  driver's modifier support checked first. *Measured (2026-10-14)*: libswscale's RGBA→NV12
  at 1080p (bilinear, BT.709) costs 3.2 ms per frame on one thread, 2.2 ms on two, 1.9 ms
  on four (FFmpeg CLI, 120 frames, read time subtracted), of the 12-13 ms encoder stage;
  the rest is the VA surface upload and the encode. Threading swscale saves ~1.3 ms and
  takes cores x264 needs; the GPU conversion removes the 3.2 ms and 60% of the readback,
  after which rendering (~10 ms) becomes the slower stage. *Done for VA-API (2026-10-14)*:
  `shaders/nv12_bt709.comp` writes BT.709 limited NV12 into a host-visible buffer
  (`read_nv12`, `nv12_pass*.cpp`), `VideoWriter::write_nv12` copies it into the encoder's
  frame; libx264 keeps RGBA. Within 1 level of a CPU conversion of the same render
  (`tests/compositor/test_nv12.cpp`). Load 11-12: encoder stage 4.7-7.8 ms per frame (was
  12-13), whole-file export 6.2 s (9.9 s before, melt 6.5 s), cuts 5.0-5.8 s (6.7 s), same
  PSNR and frames. Still one readback per frame (§7.2); the DMA-BUF hand-off stays open.
- [ ] Give libx264 the GPU-made NV12 too (it accepts `nv12` input): the software route still
  reads back RGBA and converts on the CPU, and stays 12-20% behind melt after the VA-API
  route caught up ([2026-10-15 rerun](Research/evidence/2026-10-15-cross-editor-after-export-work.txt)).
  The conversion is ~3 ms of a ~40 ms x264 frame, so expect a part of the gap, not all.

#### 11. `apps/omamovie`: QML (§11.1)

No JavaScript function in the QML is longer than 28 lines (`TimelineView.qml`'s menu
opening and follow-the-playhead handlers); models, edits and rendering stay in C++.

#### 12. `tools/oma-project`

No new items. `diff` dumps both projects through `load` and `to_json`, so formatting cannot
show up as a change, and compares lines with Myers' algorithm, O((N + M)·D) time and O(D²)
memory, refusing past 2,000 edits: right for minimal pairs (§16.6). The commands reuse
`libs/project`, `libs/importers` and `libs/ingest` (§16.7). `tools/spikes` was not reviewed.

[← Implementation plan](implementation-plan.md)
