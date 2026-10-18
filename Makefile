# CMake does the parallelism; this file must never drive two cmake invocations at once.
.NOTPARALLEL:
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DEFAULT_GOAL := all

BUILD        ?= debug
WERROR       ?= 1
RELEASE_OPT_LEVEL ?= 2
RELEASE_IPO  ?= 0
PREFIX       ?= /usr/local
DESTDIR      ?=
LICENSE_NAME ?= omamovie
V            ?= 0
FUZZ_RUNS    ?= 200000
FUZZ_PROBE_SECONDS ?= 600

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
                 -DOMA_RELEASE_OPT_LEVEL=$(RELEASE_OPT_LEVEL) \
                 -DOMA_ENABLE_RELEASE_IPO=$(if $(filter 1,$(RELEASE_IPO)),ON,OFF) \
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

include make/build.mk
include make/tools.mk
