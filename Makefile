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
  TEST_ENV   := ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1 \
                UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
else ifeq ($(BUILD),tsan)
  MODE_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=thread
  TEST_ENV   := TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1
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
include tests/base/module.mk

# -------------------------------------------------------------------------- rules

$(BUILD_DIR)/obj/%.o: %.cpp $(MAKEFILE_LIST)
	$(call say,CXX,$<)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(DEPFLAGS) $(SRCFLAGS_$<) -c $< -o $@

-include $(patsubst %.cpp,$(BUILD_DIR)/obj/%.d,$(ALL_SOURCES))

.PHONY: all libs tests test clean distclean compdb format format-check tidy fixtures spikes deps help

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

all: libs tests compdb

libs: $(ALL_LIBS)

tests: $(ALL_TESTS)

# FILTER=<pattern> runs only tests whose name contains the pattern (Cest filter).
# JUNIT_DIR=<dir> writes one JUnit report per test binary (used by CI).
test: $(ALL_TESTS)
	@failed=0; \
	for t in $(ALL_TESTS); do \
	  printf '\n== %s (%s)\n' "$${t##*/}" "$(BUILD)"; \
	  junit=""; \
	  if [ -n "$(JUNIT_DIR)" ]; then mkdir -p "$(JUNIT_DIR)"; junit="--junit $(JUNIT_DIR)/$${t##*/}-$(BUILD).xml"; fi; \
	  env $(TEST_ENV) "$$t" $$junit $(FILTER) || failed=1; \
	done; \
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

tidy: compdb
	clang-tidy -p . --quiet $(filter libs/%,$(ALL_SOURCES))

fixtures:
	tests/fixtures/generate.sh

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
	@echo '  spikes        build the M1 spikes in tools/spikes (needs Vulkan + FFmpeg headers)'
	@echo '  deps          install the dependencies declared in the PKGBUILD (uses sudo pacman)'
	@echo '  clean         remove build/$$BUILD   | distclean     remove all of build/'
	@echo 'Variables: BUILD=debug|release|asan|tsan  CXX=g++|clang++  WERROR=1|0  V=1 (verbose)'
