## Prioritized next steps — foundation and later capability

### P2 — professional foundation

7. **Prove command-state invariants.** Current `test_edit.cpp` round trip uses
   `layout()`, which omits IDs, anchors and effects. Add a test-only canonical
   printer, fixed seeds, replay and shrink around the typed Editor commands;
   compare every undo/redo state and rejected-command history behavior. Done when
   a reproduced failure yields a minimal permanent regression, without replacing
   explicit M5 cases or the command architecture.
8. **Cover audio and async boundaries.** `TimelineAudio` reuses decoders and finds
   relevant clips efficiently; tests do not establish block-partition invariance
   or playback/export PCM equality. Add exact-sample fixtures, boundary partitions
   and controlled project-close/import and rapid-seek tests; run eligible app paths
   under ASan/TSan after making them buildable there. Done when outputs align and
   jobs cancel/finish without stale publication or race reports.
9. **Measure large projects before changing structures.** M6 measured 5,000-clip
   interaction and M7 warm JSON open/save; cold reimport, multilane 10,000 clips
   and history memory are unknown. Generate 1/100/1,000/10,000-clip and 10 min/1 h/
   3 h fixtures; record edit/validate/undo, load, seek, UI, export preparation and
   recovery with hardware, build, cache state and peak memory. Change data
   structures only for a reproduced dominant cost and rerun the same benchmark.
10. **Harden SDR and output fidelity.** CPU/GPU numeric comparisons exist, while
    odd sizes, high-bit-depth paths and selected long-output effects have gaps.
    Add independent vectors and bounded GPU/CPU comparisons with stage-derived
    tolerances; externally decode representative export cuts/color/audio. Done
    when the declared SDR path agrees on validated hardware and its unsupported
    PQ/HLG behavior is visible, without an HDR claim.

### P3 — post-v0.1 capabilities, each requiring a named hand-off task

11. **Media organization and portable delivery.** Current library/relink works;
    bins, search, proxy association and collected projects remain pilots. First
    measure a multi-source hand-off task; add the smallest missing organization
    or collect operation. Test relink and decoded output after moving originals.
12. **Interchange, advanced audio/color and multicam.** OTIO/FCPXML, stems,
    scopes/HDR and multicam are not v0.1 gates. Select one by an actual receiving
    workflow; define supported semantics, cost and failure behavior, then test
    export → import semantic equality or independent delivered media. Reject a
    format or backend if the task cannot establish fidelity on available hardware.

**Scope rule:** no database, index, worker, codec, GPU backend, command layer or
timeline representation change without a named failure/workload, measured baseline,
simpler alternative and repeatable acceptance check. `ActionRegistry` already handles
discrete action IDs; add parameter schemas only for proven automation tasks. Keep
FFmpeg for codecs/containers until a concrete boundary defeats that choice.

[← Release priorities](omamovie-2026-10-12-priorities.md)
