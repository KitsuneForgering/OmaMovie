.PHONY: all configure libs tests test oma-project install run-gui clean distclean \
        compdb format format-check tidy fixtures fuzz spikes deps help

# Every target below reconfigures first: cheap when the cache is valid, and it picks up a changed
# BUILD, CXX or WERROR.
configure:
	$(Q)$(CMAKE_CONFIGURE)

all: configure
	$(Q)$(CMAKE_BUILD)
	$(Q)$(CMAKE_BUILD) --target compdb

libs: configure
	$(Q)$(CMAKE_BUILD) --target $(LIB_TARGETS)

tests: configure
	$(Q)$(CMAKE_BUILD) --target $(TEST_TARGETS)

# FILTER=<pattern> runs only the Cest tests whose name contains the pattern (through
# tests/support/run_test.sh and OMA_TEST_FILTER). JUNIT_DIR=<dir> writes one CTest JUnit report.
test: configure
	$(Q)$(CMAKE_BUILD)
	$(Q)$(if $(JUNIT_DIR),mkdir -p $(JUNIT_DIR))
	$(Q)OMA_TEST_FILTER='$(FILTER)' ctest --preset $(BUILD) \
		$(if $(JUNIT_DIR),--output-junit $(CURDIR)/$(JUNIT_DIR)/ctest-$(BUILD).xml)
# The AI runs `make ... test build/<BUILD>/omamovie`: build the app as a named target.
$(APP): configure
	$(Q)$(CMAKE_BUILD) --target omamovie

oma-project: configure
	$(Q)$(CMAKE_BUILD) --target oma-project

install: configure
	$(Q)$(CMAKE_BUILD) --target omamovie
	$(Q)DESTDIR=$(DESTDIR) cmake --install $(BUILD_DIR) --prefix $(PREFIX)

run-gui: $(APP)
	$(if $(filter 1,$(RUN_GUI_SMOKE)),$(Q)test -f tests/fixtures/generated/hevc_10bit.mp4 || $(MAKE) fixtures)
	$(Q)$(APP) $(if $(filter 1,$(RUN_GUI_SMOKE)),--smoke,$(GUI_FILE))

compdb: configure
	$(Q)$(CMAKE_BUILD) --target compdb

format: configure
	$(Q)$(CMAKE_BUILD) --target format

format-check: configure
	$(Q)$(CMAKE_BUILD) --target format-check

tidy: configure
	$(Q)$(CMAKE_BUILD) --target tidy

fixtures: configure
	$(Q)$(CMAKE_BUILD) --target fixtures
