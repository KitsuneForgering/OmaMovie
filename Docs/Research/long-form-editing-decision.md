## Conclusion and decision

OmaMovie has no product reason to impose a short-video duration limit. A two-hour file
with three cuts and a two-hour project with thousands of clips stress different parts of
the system. Keep the versioned JSON project and simdjson parser for now. Complete export
and recovery, then measure timeline size, media seeking, preview, memory and export on
declared long-form workloads. Add proxies, preview renders, indexes or a different project
store only when a measured bottleneck justifies their cost.

The first long-form promise should be **a recoverable two-hour edit on the reference Intel
machine**, with the exact source codecs and scene complexity stated. It is not a claim that
all long or professional productions work. The gate below is proposed and has not run.

[Return to long-form-editing.md](long-form-editing.md)
