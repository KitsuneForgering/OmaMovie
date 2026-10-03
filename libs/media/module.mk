# libs/media — probe and decode on top of FFmpeg (libav* confined to src/). Depends on gpu, base.
MEDIA_PKGS := libavformat libavcodec libavutil libswresample libswscale
$(eval $(call oma_library,media,$(wildcard libs/media/src/*.cpp),gpu base,$(shell pkg-config --cflags $(MEDIA_PKGS) 2>/dev/null),$(shell pkg-config --libs $(MEDIA_PKGS) 2>/dev/null)))
