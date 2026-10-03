# libs/playback — what playback renders from a timeline snapshot: the sequence's audio mix
# (decoded with libs/media, mixed with libs/audio). No Qt, so it is testable without a UI.
$(eval $(call oma_library,playback,$(wildcard libs/playback/src/*.cpp),timeline audio media base,,))
