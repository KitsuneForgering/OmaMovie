# libs/audio tests: the sample ring, the master clock and the outputs (the PipeWire case skips
# when no daemon answers, as in CI).
$(eval $(call oma_test,audio_tests,$(wildcard tests/audio/*.cpp),audio base))
