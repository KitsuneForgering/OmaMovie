# libs/project — the native project format (ADR-0007): JSON parsed with simdjson, written by a
# small pretty printer, saved atomically. Depends on the timeline model only; no Qt or FFmpeg.
PROJECT_PKGS := simdjson
$(eval $(call oma_library,project,$(wildcard libs/project/src/*.cpp),timeline base,$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(PROJECT_PKGS) 2>/dev/null)),$(shell pkg-config --libs $(PROJECT_PKGS) 2>/dev/null)))
