## Project model and sound choices

OmaMovie is a local Qt Quick video editor for Omarchy, aimed first at recordings and
phone footage. The primary data path is media probe/decode → rational timeline evaluation
→ Vulkan composition → Qt preview, with separate timeline audio mixed to PipeWire.
`Session` connects the UI to these modules. A versioned JSON document saves references
and edits. Export is still a planned path ([M7](implementation-plan.md#m7--project-and-export)).
Workload dimensions are source duration, clip count, track count, distinct media, codec/GOP,
resolution, active layers/effects and output duration; none can be replaced by one “project
size” number.

Worth preserving:

- Rational time and a sorted, nonoverlapping clip vector give exact edit boundaries and
  logarithmic clip lookup within a track ([model.cpp](../libs/timeline/src/model.cpp)).
- Commands validate the complete timeline and revert failed edits; the cost buys a strong
  current correctness boundary ([editor.cpp](../libs/timeline/src/editor.cpp)).
- The video producer has a bounded queue and drops stale prepared frames against the clock
  ([video_scheduler.cpp](../apps/omamovie/src/video_scheduler.cpp)). The frame source bounds
  decoder count and retained history ([frame_source.cpp](../apps/omamovie/src/frame_source.cpp)).
- Project input has file, depth and item limits, and saving writes a same-directory temporary
  file, syncs it, renames it and syncs the directory ([document.cpp](../libs/project/src/document.cpp)).
  That is a sound design; crash durability still needs the planned failure test.

[Return to project-analysis-2026-10-04.md](project-analysis-2026-10-04.md)
