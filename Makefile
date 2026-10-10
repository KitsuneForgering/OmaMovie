# OmaMovie — thin GNU Make front-end over the CMake build (ADR-0018).
#
#   make                  build the libs, tools, app and tests (BUILD=debug)
#   make test             build and run the tests
#   make BUILD=asan test  AddressSanitizer + UBSan     (BUILD: debug release asan tsan)
#   make help             list every target
#
# CMake owns the build (CMakeLists.txt + CMakePresets.json, the dependency graph in
# cmake/OmaDependencyGraph.cmake). This file only maps the flags and target names used by
# CLAUDE.md §3 and CI onto `cmake --preset` / `ctest --preset`, and keeps `make` the friendly
# entry point.

# CMake does the parallelism; this file must never drive two cmake invocations at once.
.NOTPARALLEL:
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DEFAULT_GOAL := all

BUILD        ?= debug
WERROR       ?= 1
PREFIX       ?= /usr/local
DESTDIR      ?=
LICENSE_NAME ?= omamovie
V            ?= 0
FUZZ_RUNS    ?= 200000

VALID_BUILDS := debug release asan tsan
ifeq ($(filter $(BUILD),$(VALID_BUILDS)),)
  $(error invalid BUILD='$(BUILD)'. Use: debug, release, asan or tsan)
endif

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

# CXX from the command line or environment (CI passes CXX=clang++); the preset keeps CMake's
# default compiler when unset. The other cache variables mirror the options the old Makefile
# exposed (WERROR, the license dir used by install()).
CMAKE_CXX_ARG := $(if $(filter undefined default,$(origin CXX)),,-DCMAKE_CXX_COMPILER=$(CXX))
# Packagers (makepkg) export CXXFLAGS/CPPFLAGS/LDFLAGS; forward them into the cache so hardening
# and LTO survive. The per-configuration flags live in CMakeLists.txt and are not overridden.
PACKAGER_ARG  := $(if $(strip $(CXXFLAGS)$(CPPFLAGS)),-DCMAKE_CXX_FLAGS='$(CPPFLAGS) $(CXXFLAGS)') \
                 $(if $(strip $(LDFLAGS)),-DCMAKE_EXE_LINKER_FLAGS='$(LDFLAGS)')
CMAKE_EXTRA   := $(CMAKE_CXX_ARG) $(PACKAGER_ARG) \
                 -DOMA_WERROR=$(if $(filter 1,$(WERROR)),ON,OFF) \
                 -DOMA_LICENSE_NAME=$(LICENSE_NAME)

BUILD_DIR  := build/$(BUILD)
APP        := $(BUILD_DIR)/omamovie
OMA_PROJECT := $(BUILD_DIR)/tools/oma-project/oma-project

CMAKE_CONFIGURE := cmake --preset $(BUILD) $(CMAKE_EXTRA)
CMAKE_BUILD      := cmake --build --preset $(BUILD)

LIB_TARGETS   := oma_base oma_gpu oma_media oma_compositor oma_audio oma_timeline \
                 oma_playback oma_project
TEST_TARGETS  := base_tests gpu_tests media_tests compositor_tests audio_tests \
                 timeline_tests playback_tests project_tests

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

# Parser fuzzing (libFuzzer, Clang): build with the fuzz preset, then run each target from its
# seed corpus in tests/fuzz/corpus. New inputs land in build/fuzz/, never in the repository.
fuzz:
	$(Q)cmake --preset fuzz
	$(Q)cmake --build --preset fuzz
	$(Q)build/fuzz/fuzz_cube -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/cube-corpus tests/fuzz/corpus/cube
	@# simdjson's inline padded_string pairs new(std::nothrow)[] with delete[]; under clang's
	@# sanitizers with libstdc++ the delete reaches free(), a toolchain mismatch outside our code.
	$(Q)ASAN_OPTIONS=alloc_dealloc_mismatch=0 build/fuzz/fuzz_project -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/project-corpus tests/fuzz/corpus/project
	@# libstdc++'s stable_sort buffer meets the same new/free mismatch under clang's sanitizers.
	$(Q)ASAN_OPTIONS=alloc_dealloc_mismatch=0 build/fuzz/fuzz_subtitles -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/subtitles-corpus tests/fuzz/corpus/subtitles

spikes:
	$(Q)cmake --preset spikes
	$(Q)cmake --build --preset spikes

clean:
	rm -rf $(BUILD_DIR)

distclean:
	rm -rf build compile_commands.json

# Installs every dependency declared in the PKGBUILD (runtime, build, check and dev tools).
deps:
	scripts/deps.sh --install

help:
	@echo 'Targets:'
	@echo '  all           libs + tools + app + tests + compile_commands.json (default)'
	@echo '  test          build and run the tests  [FILTER=pattern] [JUNIT_DIR=dir]'
	@echo '  libs | tests  build only the libraries / the test binaries'
	@echo '  oma-project   build the native project inspect/validate/dump CLI'
	@echo '  compdb        write compile_commands.json for clangd/clang-tidy'
	@echo '  format        apply clang-format     | format-check  check without changing'
	@echo '  tidy          run clang-tidy on the libs'
	@echo '  fixtures      generate test media in tests/fixtures/generated (needs ffmpeg)'
	@echo '  fuzz          run the parser fuzz targets with clang (libFuzzer) [FUZZ_RUNS=n]'
	@echo '  spikes        build the M1 spikes in tools/spikes (Vulkan + FFmpeg + Qt Quick/RHI)'
	@echo '  install       install the app tree [DESTDIR=dir PREFIX=/usr/local] (BUILD=release)'
	@echo '  run-gui       open the Qt editor shell [GUI_FILE=path; RUN_GUI_SMOKE=1]'
	@echo '  deps          install the dependencies declared in the PKGBUILD (uses sudo pacman)'
	@echo '  clean         remove build/$$BUILD   | distclean     remove all of build/'
	@echo 'Variables: BUILD=debug|release|asan|tsan  CXX=g++|clang++  WERROR=1|0  V=1 (verbose)'
