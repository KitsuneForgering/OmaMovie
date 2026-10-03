# libs/base tests (one binary, one Cest suite per file).
$(eval $(call oma_test,base_tests,$(wildcard tests/base/*.cpp),base))
