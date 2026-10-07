# OmaMovie — GNU Make build (ADR-0001).
#
#   make                  build libs and tests (BUILD=debug)
#   make test             build and run the tests
#   make BUILD=asan test  AddressSanitizer + UBSan     (BUILD: debug release asan tsan)
#   make help             list every target
#
# Non-recursive: each lib declares its sources in libs/<name>/module.mk and each test
# suite in tests/<name>/module.mk. The dependency graph from CLAUDE.md §5.2 is enforced
# here: a forbidden dependency is an error before anything compiles.

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all

BUILD     ?= debug
BUILD_DIR := build/$(BUILD)
# Honor CXX/AR from the command line or environment. The default archiver understands LTO
# objects (makepkg builds with -flto=auto): gcc-ar for GCC, llvm-ar for Clang.
ifeq ($(origin CXX),default)
  CXX := g++
endif
ifeq ($(origin AR),default)
  ifneq ($(findstring clang,$(CXX)),)
    AR := llvm-ar
  else
    AR := gcc-ar
  endif
endif
WERROR    ?= 1
V         ?= 0

ifeq ($(V),1)
  Q :=
  say :=
else
  Q := @
  say = @printf '  %-6s %s\n' $(1) $(2)
endif

# ----------------------------------------------------------------------------- flags

ifeq ($(BUILD),debug)
  MODE_FLAGS := -O0 -g3
else ifeq ($(BUILD),release)
  MODE_FLAGS := -O2 -g -DNDEBUG
else ifeq ($(BUILD),asan)
  MODE_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
  # Drivers stay loaded and stacks are fully unwound, so the suppressions match their frames.
  TEST_ENV   := ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1:fast_unwind_on_malloc=0 \
                LSAN_OPTIONS=suppressions=$(CURDIR)/tests/support/lsan.supp:print_suppressions=0 \
                UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
                VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1
else ifeq ($(BUILD),tsan)
  MODE_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=thread
  TEST_ENV   := TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:suppressions=$(CURDIR)/tests/support/tsan.supp \
                VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1
else
  $(error invalid BUILD='$(BUILD)'. Use: debug, release, asan or tsan)
endif

# CPPFLAGS/CXXFLAGS/LDFLAGS from the environment (e.g. makepkg's distribution flags) are
# appended, so packagers can add hardening and LTO without editing this file.
CXXFLAGS_BASE := -std=c++23 -pthread $(MODE_FLAGS) $(CPPFLAGS) $(CXXFLAGS)
DEPFLAGS      := -MMD -MP
LDFLAGS_BASE  := -pthread $(filter -fsanitize=%,$(MODE_FLAGS)) $(LDFLAGS)

# Project libraries (CLAUDE.md §22).
LIB_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast \
                -Wcast-align -Wconversion -Wsign-conversion -Wnull-dereference \
                -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
# GCC 16 reports -Wnull-dereference inside libstdc++'s unordered_map::find once
# -fsanitize=thread instruments it (a false positive); the other builds keep the check.
ifeq ($(BUILD),tsan)
  LIB_WARNINGS := $(filter-out -Wnull-dereference,$(LIB_WARNINGS))
endif
# Tests expand Cest macros in their own files (C-style casts, implicit conversions).
TEST_WARNINGS := -Wall -Wextra -Wshadow

ifeq ($(WERROR),1)
  LIB_WARNINGS  += -Werror
  TEST_WARNINGS += -Werror
endif

# ------------------------------------------------------------------- module graph

# Allowed dependencies per lib (CLAUDE.md §5.2). A new lib must be registered here.
ALLOWED_DEPS_base       :=
ALLOWED_DEPS_gpu        := base
ALLOWED_DEPS_media      := base gpu
ALLOWED_DEPS_compositor := base gpu media
ALLOWED_DEPS_audio      := base
ALLOWED_DEPS_timeline   := base
ALLOWED_DEPS_playback   := base gpu media audio timeline
ALLOWED_DEPS_project-ir := base
ALLOWED_DEPS_importers  := base project-ir
ALLOWED_DEPS_project    := base timeline project-ir

ALL_SOURCES :=
ALL_LIBS    :=
ALL_TESTS   :=

obj_of = $(patsubst %.cpp,$(BUILD_DIR)/obj/%.o,$(1))

# $(call oma_library,<name>,<sources>,<project deps>,<external flags>,<external libs>)
define oma_library
$$(if $$(filter undefined,$$(origin ALLOWED_DEPS_$(1))),$$(error lib '$(1)' is not registered in the Makefile (ALLOWED_DEPS_$(1)); see CLAUDE.md §5.2))
$$(foreach d,$(3),$$(if $$(filter $$(d),$$(ALLOWED_DEPS_$(1))),,$$(error lib '$(1)': forbidden dependency on '$$(d)'. Allowed: [$$(ALLOWED_DEPS_$(1))] (CLAUDE.md §5.2))))
INC_$(1)  := -Ilibs/$(1)/include $$(foreach d,$(3),$$(INC_$$(d))) $(4)
LIB_$(1)  := $(BUILD_DIR)/lib/liboma_$(1).a
LINK_$(1) := $$(LIB_$(1)) $$(foreach d,$(3),$$(LINK_$$(d))) $(5)
$$(foreach s,$(2),$$(eval SRCFLAGS_$$(s) := $$(LIB_WARNINGS) $$(INC_$(1)) -Ilibs/$(1)/src))
$$(LIB_$(1)): $(call obj_of,$(2))
	$$(call say,AR,$$@)
	@mkdir -p $$(@D)
	$(Q)rm -f $$@ && $(AR) rcs $$@ $$^
ALL_SOURCES += $(2)
ALL_LIBS    += $$(LIB_$(1))
endef

# $(call oma_test,<name>,<sources>,<project libs>)
define oma_test
TEST_BIN_$(1) := $(BUILD_DIR)/tests/$(1)
$$(foreach s,$(2),$$(eval SRCFLAGS_$$(s) := $$(TEST_WARNINGS) $$(foreach l,$(3),$$(INC_$$(l))) -Itests/support -Ithird_party/cest))
$$(TEST_BIN_$(1)): $(call obj_of,$(2)) $$(foreach l,$(3),$$(LIB_$$(l)))
	$$(call say,LINK,$$@)
	@mkdir -p $$(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(call obj_of,$(2)) $$(foreach l,$(3),$$(LINK_$$(l))) $(LDFLAGS_BASE) -o $$@
ALL_SOURCES += $(2)
ALL_TESTS   += $$(TEST_BIN_$(1))
endef

include libs/base/module.mk
include libs/gpu/module.mk
include libs/media/module.mk
include libs/compositor/module.mk
include libs/audio/module.mk
include libs/timeline/module.mk
include libs/playback/module.mk
include libs/project/module.mk
include tests/base/module.mk
include tests/gpu/module.mk
include tests/media/module.mk
include tests/compositor/module.mk
include tests/audio/module.mk
include tests/timeline/module.mk
include tests/playback/module.mk
include tests/project/module.mk

# -------------------------------------------------------------------------- rules

$(BUILD_DIR)/obj/%.o: %.cpp $(MAKEFILE_LIST)
	$(call say,CXX,$<)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(DEPFLAGS) $(SRCFLAGS_$<) -c $< -o $@

-include $(patsubst %.cpp,$(BUILD_DIR)/obj/%.d,$(ALL_SOURCES))

.PHONY: install all libs tests test clean distclean compdb format format-check tidy fixtures fuzz spikes run-gui oma-project deps help

# Spikes (tools/spikes/*.cpp): disposable single-file experiments (Docs/spikes/), built only on
# request because they need the Vulkan and FFmpeg development files. pkg-config runs inside the
# recipe so a missing package never affects other targets.
SPIKE_PKGS := vulkan libavformat libavcodec libavutil
SPIKE_BINS := $(patsubst tools/spikes/%.cpp,$(BUILD_DIR)/spikes/%,$(wildcard tools/spikes/*.cpp))

$(BUILD_DIR)/spikes/%: tools/spikes/%.cpp $(MAKEFILE_LIST)
	$(call say,SPIKE,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) -Wall -Wextra $$(pkg-config --cflags $(SPIKE_PKGS)) $< -o $@ \
		$$(pkg-config --libs $(SPIKE_PKGS)) $(LDFLAGS_BASE)

spikes: $(SPIKE_BINS)

# S5 drives the library decoder and compositor directly.
$(BUILD_DIR)/spikes/s5_minimal_compositing: tools/spikes/s5_minimal_compositing.cpp $(LIB_compositor) $(LIB_media) $(LIB_gpu) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,SPIKE,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(TEST_WARNINGS) $(INC_compositor) $< -o $@ $(LINK_compositor) $(LDFLAGS_BASE)

# S6 imports OmaMovie's device into libplacebo (development dependency only, ADR-0006).
$(BUILD_DIR)/spikes/s6_libplacebo: tools/spikes/s6_libplacebo.cpp $(LIB_compositor) $(LIB_media) $(LIB_gpu) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,SPIKE,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) -Wall -Wextra $(INC_compositor) $$(pkg-config --cflags libplacebo) \
		$< -o $@ $(LINK_compositor) $$(pkg-config --libs libplacebo) $(LDFLAGS_BASE)

# S4 reuses the real compositor and Qt's versioned RHI headers (limited compatibility API).
# Keep Qt discovery in this recipe: core library/test builds do not require Qt.
$(BUILD_DIR)/spikes/s4_qt_shared_device: tools/spikes/s4_qt_shared_device.cpp $(LIB_compositor) $(LIB_media) $(LIB_gpu) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,SPIKE,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(TEST_WARNINGS) $(INC_compositor) \
		$$(pkg-config --cflags Qt6Quick) \
		-isystem $$(pkg-config --variable=includedir Qt6Gui)/QtGui/$$(pkg-config --modversion Qt6Gui) \
		-isystem $$(pkg-config --variable=includedir Qt6Gui)/QtGui/$$(pkg-config --modversion Qt6Gui)/QtGui \
		$< -o $@ $(LINK_compositor) $$(pkg-config --libs Qt6Quick) $(LDFLAGS_BASE)

# The app: every source in apps/omamovie/src plus moc output for headers declaring Q_OBJECT.
# Built in one compiler call; QML is loaded from the source tree at run time.
APP_SOURCES := $(wildcard apps/omamovie/src/*.cpp apps/omamovie/src/platform/omarchy/*.cpp)
APP_HEADERS := $(wildcard apps/omamovie/src/*.hpp apps/omamovie/src/platform/omarchy/*.hpp)
APP_MOC_HEADERS := $(shell grep -l Q_OBJECT $(APP_HEADERS) 2>/dev/null)
APP_MOCS := $(patsubst apps/omamovie/src/%.hpp,$(BUILD_DIR)/gen/omamovie/moc_%.cpp,$(APP_MOC_HEADERS))

$(BUILD_DIR)/gen/omamovie/moc_%.cpp: apps/omamovie/src/%.hpp $(MAKEFILE_LIST)
	$(call say,MOC,$<)
	@mkdir -p $(@D)
	$(Q)/usr/lib/qt6/moc $< -o $@

# The viewer wraps compositor images with Qt's RHI (limited compatibility API, versioned headers).
$(BUILD_DIR)/omamovie: $(APP_SOURCES) $(APP_HEADERS) $(APP_MOCS) $(wildcard apps/omamovie/qml/*.qml) $(LIB_project) $(LIB_playback) $(LIB_timeline) $(LIB_audio) $(LIB_compositor) $(LIB_media) $(LIB_gpu) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,GUI,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) -fPIC $(TEST_WARNINGS) $(INC_project) $(INC_playback) $(INC_compositor) -Iapps/omamovie/src \
		$$(pkg-config --cflags Qt6Quick Qt6Test Qt6DBus) \
		-isystem $$(pkg-config --variable=includedir Qt6Gui)/QtGui/$$(pkg-config --modversion Qt6Gui)/QtGui \
		$(APP_SOURCES) $(APP_MOCS) -o $@ $(LINK_project) $(LINK_playback) $(LINK_compositor) \
		$$(pkg-config --libs Qt6Quick Qt6Test Qt6DBus) $(LDFLAGS_BASE)

# Headless Session tests: the app's sources without main.cpp, Qt's offscreen platform. Run by
# `make test` in debug and release (Qt's own allocations make sanitizer runs noise).
APP_TEST_SOURCES := $(wildcard tests/app/*.cpp)
APP_TESTS := $(BUILD_DIR)/tests/app_tests
$(APP_TESTS): $(APP_TEST_SOURCES) $(APP_SOURCES) $(APP_HEADERS) $(APP_MOCS) $(LIB_project) $(LIB_playback) $(LIB_timeline) $(LIB_audio) $(LIB_compositor) $(LIB_media) $(LIB_gpu) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,LINK,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) -fPIC $(TEST_WARNINGS) $(INC_project) $(INC_playback) $(INC_compositor) -Iapps/omamovie/src \
		-Itests/support -Ithird_party/cest $$(pkg-config --cflags Qt6Quick Qt6Test Qt6DBus) \
		-isystem $$(pkg-config --variable=includedir Qt6Gui)/QtGui/$$(pkg-config --modversion Qt6Gui)/QtGui \
		$(filter-out apps/omamovie/src/main.cpp,$(APP_SOURCES)) $(APP_MOCS) $(APP_TEST_SOURCES) -o $@ \
		$(LINK_project) $(LINK_playback) $(LINK_compositor) $$(pkg-config --libs Qt6Quick Qt6Test Qt6DBus) $(LDFLAGS_BASE)
ifneq ($(filter debug release,$(BUILD)),)
  ALL_APP_TESTS := $(APP_TESTS)
endif

# The installed tree (PKGBUILD package() and the release tarball share it): the executable finds
# its QML in ../share/omamovie/qml. Build with BUILD=release first.
PREFIX  ?= /usr/local
DESTDIR ?=
# The license directory; Arch packages pass their pkgname.
LICENSE_NAME ?= omamovie
install: $(BUILD_DIR)/omamovie
	install -Dm755 $(BUILD_DIR)/omamovie "$(DESTDIR)$(PREFIX)/bin/omamovie"
	install -Dm644 -t "$(DESTDIR)$(PREFIX)/share/omamovie/qml" apps/omamovie/qml/*.qml
	install -Dm644 apps/omamovie/data/omamovie.desktop "$(DESTDIR)$(PREFIX)/share/applications/omamovie.desktop"
	install -Dm644 apps/omamovie/data/omamovie.svg "$(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/omamovie.svg"
	install -Dm644 apps/omamovie/data/omamovie-mime.xml "$(DESTDIR)$(PREFIX)/share/mime/packages/omamovie.xml"
	install -Dm644 LICENSE "$(DESTDIR)$(PREFIX)/share/licenses/$(LICENSE_NAME)/LICENSE"

run-gui: $(BUILD_DIR)/omamovie
	$(if $(filter 1,$(RUN_GUI_SMOKE)),@test -f tests/fixtures/generated/hevc_10bit.mp4 || $(MAKE) fixtures)
	$(Q)$(BUILD_DIR)/omamovie $(if $(filter 1,$(RUN_GUI_SMOKE)),--smoke,$(GUI_FILE))

$(BUILD_DIR)/tools/oma-project/oma-project: tools/oma-project/main.cpp $(LIB_project) $(LIB_timeline) $(LIB_base) $(MAKEFILE_LIST)
	$(call say,CLI,$@)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(LIB_WARNINGS) $(INC_project) $< -o $@ $(LINK_project) $(LDFLAGS_BASE)

oma-project: $(BUILD_DIR)/tools/oma-project/oma-project

all: libs tests compdb

libs: $(ALL_LIBS)

tests: $(ALL_TESTS)

# FILTER=<pattern> runs only tests whose name contains the pattern (Cest filter).
# JUNIT_DIR=<dir> writes one JUnit report per test binary (used by CI).
test: $(ALL_TESTS) $(ALL_APP_TESTS) $(TEST_PREREQS) $(BUILD_DIR)/tools/oma-project/oma-project
	@failed=0; \
	for t in $(ALL_TESTS) $(ALL_APP_TESTS); do \
	  printf '\n== %s (%s)\n' "$${t##*/}" "$(BUILD)"; \
	  junit=""; \
	  if [ -n "$(JUNIT_DIR)" ]; then mkdir -p "$(JUNIT_DIR)"; junit="--junit $(JUNIT_DIR)/$${t##*/}-$(BUILD).xml"; fi; \
	  env $(TEST_ENV) "$$t" $$junit $(FILTER) || failed=1; \
	done; \
	if [ -z "$(FILTER)" ]; then sh tests/project/test_cli.sh $(BUILD_DIR)/tools/oma-project/oma-project || failed=1; fi; \
	exit $$failed

clean:
	rm -rf $(BUILD_DIR)

distclean:
	rm -rf build compile_commands.json

# compile_commands.json at the root for clangd and clang-tidy (flags of the current BUILD).
comma := ,
compdb_entry = {"directory":"$(CURDIR)","file":"$(CURDIR)/$(1)","command":"$(CXX) $(CXXFLAGS_BASE) $(SRCFLAGS_$(1)) -c $(1) -o $(call obj_of,$(1))"}

compdb:
	$(file >compile_commands.json,[)
	$(if $(ALL_SOURCES),$(file >>compile_commands.json,$(call compdb_entry,$(firstword $(ALL_SOURCES)))))
	$(foreach s,$(wordlist 2,$(words $(ALL_SOURCES)),$(ALL_SOURCES)),$(file >>compile_commands.json,$(comma)$(call compdb_entry,$(s))))
	$(file >>compile_commands.json,])
	@:

FORMAT_FILES = $(shell find libs tests -name '*.cpp' -o -name '*.hpp')

format:
	clang-format -i $(FORMAT_FILES)

format-check:
	clang-format --dry-run --Werror $(FORMAT_FILES)

# Generated sources (SPIR-V includes) must exist before clang-tidy parses the files using them.
tidy: compdb $(TIDY_PREREQS)
	clang-tidy -p . --quiet $(filter libs/%,$(ALL_SOURCES))

fixtures:
	tests/fixtures/generate.sh

# libFuzzer targets for parsers of untrusted files (CLAUDE.md §18). Clang only; each target
# compiles its parser with the sanitizers and starts from the seeds in tests/fuzz/corpus/.
# New inputs go to build/fuzz/, never into the repository.
FUZZ_CXX   ?= clang++
FUZZ_RUNS  ?= 200000
FUZZ_FLAGS := -std=c++23 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
              -Ilibs/base/include -Ilibs/compositor/include

build/fuzz/fuzz_cube: tests/fuzz/fuzz_cube.cpp libs/compositor/src/grade.cpp libs/base/src/error.cpp \
                      libs/compositor/include/oma/compositor/grade.hpp $(MAKEFILE_LIST)
	$(call say,FUZZ,$@)
	@mkdir -p $(@D)/cube-corpus
	$(Q)$(FUZZ_CXX) $(FUZZ_FLAGS) $(filter %.cpp,$^) -o $@

PROJECT_FUZZ_SOURCES := tests/fuzz/fuzz_project.cpp $(wildcard libs/project/src/*.cpp) \
                        $(wildcard libs/timeline/src/*.cpp) $(wildcard libs/base/src/*.cpp)
build/fuzz/fuzz_project: $(PROJECT_FUZZ_SOURCES) $(MAKEFILE_LIST)
	$(call say,FUZZ,$@)
	@mkdir -p $(@D)/project-corpus
	$(Q)$(FUZZ_CXX) $(FUZZ_FLAGS) -Ilibs/timeline/include -Ilibs/timeline/src -Ilibs/project/include \
		$(filter %.cpp,$^) -o $@ $$(pkg-config --libs simdjson)

build/fuzz/fuzz_subtitles: tests/fuzz/fuzz_subtitles.cpp libs/project/src/subtitles.cpp libs/base/src/error.cpp \
                           libs/project/include/oma/project/subtitles.hpp $(MAKEFILE_LIST)
	$(call say,FUZZ,$@)
	@mkdir -p $(@D)/subtitles-corpus
	$(Q)$(FUZZ_CXX) $(FUZZ_FLAGS) -Ilibs/project/include $(filter %.cpp,$^) -o $@

fuzz: build/fuzz/fuzz_cube build/fuzz/fuzz_project build/fuzz/fuzz_subtitles
	build/fuzz/fuzz_cube -runs=$(FUZZ_RUNS) -max_len=65536 build/fuzz/cube-corpus tests/fuzz/corpus/cube
	@# simdjson's inline padded_string pairs new(std::nothrow)[] with delete[]; under clang's
	@# sanitizers with libstdc++ the delete reaches free(), a toolchain mismatch outside our code
	@# (which allocates no arrays with new[]).
	ASAN_OPTIONS=alloc_dealloc_mismatch=0 build/fuzz/fuzz_project -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/project-corpus tests/fuzz/corpus/project
	@# libstdc++'s stable_sort buffer meets the same new/free mismatch under clang's sanitizers.
	ASAN_OPTIONS=alloc_dealloc_mismatch=0 build/fuzz/fuzz_subtitles -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/subtitles-corpus tests/fuzz/corpus/subtitles

# Installs every dependency declared in the PKGBUILD (runtime, build, check and dev tools).
deps:
	scripts/deps.sh --install

help:
	@echo 'Targets:'
	@echo '  all           libs + tests + compile_commands.json (default)'
	@echo '  test          build and run the tests  [FILTER=pattern] [JUNIT_DIR=dir]'
	@echo '  libs | tests  build only'
	@echo '  compdb        write compile_commands.json for clangd/clang-tidy'
	@echo '  format        apply clang-format     | format-check  check without changing'
	@echo '  tidy          run clang-tidy on the libs'
	@echo '  fixtures      generate test media in tests/fixtures/generated (needs ffmpeg)'
	@echo '  fuzz          run the parser fuzz targets with clang (libFuzzer) [FUZZ_RUNS=n]'
	@echo '  spikes        build the M1 spikes in tools/spikes (Vulkan + FFmpeg + Qt Quick/RHI)'
	@echo '  install       install the app tree [DESTDIR=dir PREFIX=/usr/local] (BUILD=release)'
	@echo '  run-gui       open the Qt editor shell [GUI_FILE=path; RUN_GUI_SMOKE=1]'
	@echo '  oma-project   build the native project inspect/validate/dump CLI'
	@echo '  deps          install the dependencies declared in the PKGBUILD (uses sudo pacman)'
	@echo '  clean         remove build/$$BUILD   | distclean     remove all of build/'
	@echo 'Variables: BUILD=debug|release|asan|tsan  CXX=g++|clang++  WERROR=1|0  V=1 (verbose)'
