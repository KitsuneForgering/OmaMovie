### Item-by-item code review, part 3 (2026-10-14)

Continues [part 2](implementation-plan-code-review-2.md). Base revision `0e12ece`.

#### 6. `libs/playback`

No new items. `TimelineAudio::render` binary-searches each track for the first clip that
can sound and stops after the block, O(log clips + clips in the block); a split's right
half takes over the left half's decoder without a seek (`timeline_audio.cpp:164`);
streams untouched in a pass are dropped at its end, after every clip of the pass had the
chance to take one over, so the decoder count follows what is audible; a failed decoder
is reported once and then stays silent. Opening a decoder at a cut runs synchronously on
the producer thread, covered by its 0.25 s lead; the audits record no underruns, so
pre-opening the next clip is not justified yet. `Waveform::peak_between` is a linear max
over the queried buckets; one paint covers each visible bucket once.

#### 7. `libs/project-ir` and `libs/importers`

Sound as written: the OTIO reader caps input (64 MiB, depth 64, 100,000 items), keeps every
unread object as an opaque payload with a report entry (§16.3), converts NTSC rates to exact
fractions and reports any rounding; `ir::validate` indexes media once.

- [x] Index OTIO media by target while importing. `media_of` reuses a reference by a linear
  search of the media read so far (`libs/importers/src/otio_media.cpp:37`), so a timeline
  whose clips point at distinct files costs O(clips × files) string comparisons: about
  5 · 10⁹ at the 100,000-item cap, enough to stall an import that the size checks accepted.
  *Fixed (2026-10-14)*: `Context::media_by_target`; `tests/importers/test_otio_scale.cpp`
  imports 50,000 clips of distinct files in 0.78 s (debug) against 34.8 s before, with a
  10 s bound, and checks that clips of one file still share one media entry.

#### 8. `libs/project`

Sound as written: `save` follows §14 step by step (same-directory `mkstemp`, full `write`
loop with `EINTR`, file `fsync`, `rename`, directory `fsync`, each error checked and the
temporary removed); `load` caps the file size before reading; unknown top-level fields are
kept as minified JSON for a newer writer.

- [x] Keep a project file's permissions when saving over it. `save` sets 0644 on every
  write (`libs/project/src/document.cpp:1326`), so a project the user restricted to 0600
  becomes readable by every local account at the next save or autosave, and a new file
  ignores the user's umask. *Fixed (2026-10-14)*: `detail::saved_file_mode` keeps the
  existing mode, else 0666 under the umask read from `/proc/self/status` (`umask(2)` would
  race other threads); `tests/project/test_save_mode.cpp` covers both.

#### 9. `libs/ingest`

- [x] Check an OTIO file's size and readability before reading it. `import_otio_file` reads
  the whole file through a `stringstream` (`libs/ingest/src/timeline_import.cpp:17`) and
  only then does `import_otio` refuse anything over 64 MiB, so a large file picked by
  mistake is read fully, held twice, and a missing or unreadable one reports "not JSON".
  *Fixed (2026-10-14)*: `detail::read_timeline_text` sizes the file against the public
  `importers::kMaxOtioBytes`, reads once and reports I/O errors (CLI test: a missing file
  says "cannot read the timeline file"). Media are
  then probed and fingerprinted one after another on the job worker: O(media) opens,
  acceptable until an import of many files is measured as slow.

Continued in [part 4](implementation-plan-code-review-4.md).

[← Implementation plan](implementation-plan.md)
