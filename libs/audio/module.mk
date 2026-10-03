# libs/audio — sample ring, audio clock and outputs (PipeWire, null). Depends on base only.
# PipeWire/SPA headers are C with heavy macros: include them as system headers.
AUDIO_PKGS := libpipewire-0.3
$(eval $(call oma_library,audio,$(wildcard libs/audio/src/*.cpp),base,$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(AUDIO_PKGS) 2>/dev/null)),$(shell pkg-config --libs $(AUDIO_PKGS) 2>/dev/null)))
