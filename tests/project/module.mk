# libs/project tests: save/load round trips, refused and broken files, atomic replacement.
$(eval $(call oma_test,project_tests,$(wildcard tests/project/*.cpp),project timeline base))
