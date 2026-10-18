### Item-by-item code review, part 2 (2026-10-14)

Continues [part 1](implementation-plan-code-review.md). Base revision `0e12ece`.

#### 3. `libs/media`

Sound as written: `VideoDecoder::seek` lands on the frame on screen by PTS, skips
non-reference frames that end before the target and decodes the stretch again without
skipping when a skipped frame would have been the answer, so the speedup never costs
exactness; only the chosen frame is imported or converted. Forward steps do not seek: the
app's `FrameSource` advances a decoder that already reaches the target and keeps a history
for stepping back (`apps/omamovie/src/frame_source.cpp:117`). The writer reuses its frames
(`av_frame_make_writable`) and checks the trailer and close.

- [x] Keep decoded audio on its timestamps after a gap. `AudioDecoder` anchored on the first
  frame after open or seek and then only counted samples (`audio_decoder.cpp`), so audio after
  a timestamp gap (dropped packets, joined captures) played early until the next seek.
  *Fixed (2026-10-14)*: confirmed with `tone_gap.mkv` (decoding ended at 1.5 s of 2 s, the gap
  played sound); `detail::place_frame` fills gaps over 20 ms with silence, drops overlaps and
  re-anchors past 10 s (`tests/media/test_audio_gap.cpp`); two real AAC recordings still pass
  their sync round trip. The denoise path still counts samples (not changed).
- Not worth changing now: `receive()` allocates one `AVFrame` per frame and the audio
  paths one sample vector per buffer (`audio_decoder.cpp:173`, `:490`); both run on decode
  workers, not the real-time callback, and no profile points at them.

#### 4. `libs/audio`

No new items. The real-time callback (`src/pipewire_output.cpp:147`) only dequeues, reads
the SPSC ring, zero-fills a short read and publishes counters; it neither allocates nor
locks, and a flush is a mark the callback applies, so a seek never waits a quantum. The
`frames_played` seqlock is correct as written: every field store is a release and every
read an acquire, so a reader that sees a new field also sees the odd sequence and retries.
The clock derives position from consumed frames minus reported delay, as §12 requires.
`mix_planar` evaluates gain per sample (a binary search over automation keys, a sine or
cosine inside a crossfade): O(frames · log keys) per sounding clip, on the producer
thread 0.25 s ahead of the device. An incremental key cursor would make it O(frames +
keys); leave it until a profile of many overlapping automated clips shows it.

#### 5. `libs/timeline`

Sound as written: commands store deltas (one clip, one shift), not timeline copies, so the
history grows with edits, not with project size; evaluation per frame binary-searches each
track (`Timeline::clip_at`); reattaching connected clips already uses one index per edit.

- [x] Make `Timeline::validate()` linear. `Editor::execute` runs it after every edit; it found
  each clip's media by a linear scan (O(clips × media)) and each connected clip's primary by
  scanning every track (O(connected × clips)). *Fixed (2026-10-14)*: one media map and one
  clip index per call, anchors checked after indexing (a primary may sit on a later track).
  Measured, release, 20 calls: 6,000 clips with 2,000 connected 8.3 → 4.1 ms per call;
  1,500 with 500 connected 1.06 → 0.92 ms. The earlier "~12 M visits" estimate overstated
  the cost; the gain is real but modest. Timeline and property tests pass unchanged.

Continued in [part 3](implementation-plan-code-review-3.md).

[← Implementation plan](implementation-plan.md)
