## Question, decision and budget

Question: which research premises are reliable enough to guide the next implementation?
Target: the actual C++/FFmpeg/Vulkan/Qt/PipeWire editor on Omarchy, initially the reference
Intel machine. Baseline: current library code plus software decode/upload, conventional
track/inspector UI, and existing Kdenlive/Shotcut. Wrong GPU/sync/color assumptions risk
corrupt output and costly rework; wrong audience assumptions risk building unused features.

Initial hypotheses:

| Hypothesis | Evidence that would change it |
|---|---|
| H1: keeping frames GPU-accessible helps editing | Same-output end-to-end benchmark shows interop/scheduling cost outweighs transfer savings |
| H2: one owned device simplifies decode/compositor/Qt | Version-specific queue/lifetime contracts conflict or simpler handoff meets the same budget |
| H3: Vulkan Compute suffices for initial effects | A concrete effect/library requires another backend with measured total advantage |
| H4: contextual/magnetic UI helps simple edits | Task pilot shows hidden controls, surprise track moves or worse recovery than baseline |
| H5: local consumer-media workflows differentiate OmaMovie | Existing Linux tools perform the same tasks with comparable setup/performance on target hardware |

Budget: one repository-wide document inventory, primary-source inspection in technical and
product/platform passes, focused counterevidence checks, local source/environment inspection,
capacity/time arithmetic, and document consistency checks. Coverage favors consequential
premises over release trivia. No exhaustive literature review, GPU matrix rerun, competitor
installation, pricing/legal review or user study is claimed. Stop when the next decisions
are supported or explicitly gated; unavailable NVIDIA/AMD hardware and pending Qt/color
experiments prevent broader validation. [Source register](sources.md) records actual access.

[Return to skeptical-review.md](skeptical-review.md)
