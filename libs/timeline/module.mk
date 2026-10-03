# libs/timeline — timeline model, edit commands with undo/redo, and per-instant evaluation.
# Depends on base only: no Qt, GPU or FFmpeg (CLAUDE.md §5.2, §10).
$(eval $(call oma_library,timeline,$(wildcard libs/timeline/src/*.cpp),base,,))
