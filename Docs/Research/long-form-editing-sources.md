## Research record and limits

Budget: one local code pass over project, timeline, viewer, waveform and QML paths;
four targeted online search passes covering FFmpeg, Qt, Kdenlive, SQLite and the SEI;
one arithmetic check. Stopped because the next decision is a workload benchmark, not another feature
list. No long-form runtime benchmark, user study, export or failure-injection test ran.
The arithmetic was executed locally with Python on 2026-10-04; its two-hour result is
`7200 × 100 × 2 × 4 / 2^20 = 5.49 MiB`. Online sources were inspected as HTML on
2026-10-04 except the SEI PDF, whose relevant Appendix A passage was available in text.
The cited FFmpeg API is 8.1 documentation; Qt pages are 6.12 unless their URL says
otherwise; Kdenlive pages are its 26.08 manual. Page update dates not shown are unknown.
The main uncertainty is the workload distribution and measured bottleneck on OmaMovie's
reference hardware.

[Return to long-form-editing.md](long-form-editing.md)
