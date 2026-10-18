## Prioritized next steps — release path

Each item names a problem, evidence, smallest change, check and completion rule.
Priority applies to the declared Intel/software v0.1 scope. The uncommitted
`main.cpp` build error is a current-workspace blocker, not a defect attributed to
HEAD: fix its pointer mismatch before treating a new `make test` result as green.

### P0 — correctness and reliability blockers

1. **Enforce pull-request CI.** The workflow runs on PRs (`.github/workflows/ci.yml`),
   but GitHub returned no `master` protection or ruleset on 2026-10-12. Require the
   existing test/lint/fuzz checks before merge; verify with GitHub ruleset readback
   and a draft PR whose failed check blocks merging. Keep GCC/Clang and sanitizer
   matrix. This is a repository-setting change, not a new test framework.
2. **Close export content and publication gates.** `tests/app/test_export_crash.cpp`
   covers process-crash rename checkpoints; M7 still lacks write/sync injection and
   selected decoded frames/samples from a slowed project-switch export. Add focused
   fault seams around existing writes and an external ffmpeg/ffprobe oracle. Done
   when old outputs survive each injected failure and successful movie/sidecar
   match the pinned snapshot in frame, audio and timing content.
3. **Make the v0.1 edit task provable.** `test_edit_workflow.cpp` checks a marked
   range and 35-frame output; M6 still lacks a full visible dialogue/cutaway task.
   Exercise import → source marks → insert/overwrite → trim/roll/slip/slide → playback
   → save/reopen → export, inspecting selected frames/samples externally and both
   model and UI state. Done when the task completes with no silent selection or
   media change on the baseline hardware; record corrections and latency.

### P1 — workflow blockers for v0.1

4. **Declare retimed-audio behavior at the action.** `TimelineAudio::mix_clip`
   silences non-1× maps (`timeline_audio.cpp:177`); M8 documents this. For v0.1,
   present an explicit video-only/silent-audio choice or disallow an ambiguous
   speed action on audible clips. Test preview/export, undo and save/reopen with
   tone pulses. Constant-speed audio mapping is a later feature unless the target
   task needs it; done when users cannot silently lose expected sound.
5. **Prove installed recovery on Omarchy.** M7 has staged installation and
   process-crash tests, not a disposable installed PKGBUILD launch. Run install,
   project reopen, missing-media relink, autosave restore and export recovery with
   generated media. Done when the installed binary completes the same workflow
   without relying on source-tree QML or fixtures.
6. **Automate a small real QML path.** `app_tests` tests Session offscreen, while
   the QML smoke lives in `main.cpp` and has an intermittent menu route (M6).
   Move a reusable smoke driver out of production startup, reuse ActionRegistry
   and stable object names, and check Razor/Undo, source marks and an effect in
   both model and visible controls. Done when a bounded offscreen workflow runs
   in PR CI; keep the native Wayland check for compositor-specific behavior.

[Professional foundation and later capabilities](omamovie-2026-10-12-foundation.md)

[← Assessment](omamovie-2026-10-12.md)
