# libs/playback tests: the timeline audio mix against generated fixtures (no audio device).
$(eval $(call oma_test,playback_tests,$(wildcard tests/playback/*.cpp),playback timeline audio media base))
