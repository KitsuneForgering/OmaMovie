# OmaMovie — build com GNU Make (ADR-0001).
#
#   make                  compila libs e testes (BUILD=debug)
#   make test             compila e roda os testes
#   make BUILD=asan test  AddressSanitizer + UBSan     (BUILD: debug release asan tsan)
#   make help             lista todos os alvos
#
# Não recursivo: cada lib declara seus fontes em libs/<nome>/module.mk e cada suíte de
# testes em tests/<nome>/module.mk. O grafo de dependências do CLAUDE.md §5.2 é
# verificado aqui: uma dependência proibida é erro antes de compilar qualquer coisa.

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all

BUILD     ?= debug
BUILD_DIR := build/$(BUILD)
# Respeita CXX/AR vindos da linha de comando ou do ambiente; senão usa g++/ar.
ifeq ($(origin CXX),default)
  CXX := g++
endif
ifeq ($(origin AR),default)
  AR := ar
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
  $(error BUILD='$(BUILD)' inválido. Use: debug, release, asan ou tsan)
endif

CXXFLAGS_BASE := -std=c++23 -pthread $(MODE_FLAGS)
DEPFLAGS      := -MMD -MP
LDFLAGS_BASE  := -pthread $(filter -fsanitize=%,$(MODE_FLAGS))

# Libs do projeto (CLAUDE.md §22).
LIB_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast \
                -Wcast-align -Wconversion -Wsign-conversion -Wnull-dereference \
                -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
# Testes expandem macros do Cest no próprio arquivo (casts no estilo C, conversões).
TEST_WARNINGS := -Wall -Wextra -Wshadow

ifeq ($(WERROR),1)
  LIB_WARNINGS  += -Werror
  TEST_WARNINGS += -Werror
endif

# ----------------------------------------------------------------- grafo de módulos

# Dependências permitidas por lib (CLAUDE.md §5.2). Uma lib nova precisa estar aqui.
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

# $(call oma_library,<nome>,<fontes>,<deps do projeto>,<flags externas>,<libs externas>)
define oma_library
$$(if $$(filter undefined,$$(origin ALLOWED_DEPS_$(1))),$$(error lib '$(1)' não registrada no Makefile (ALLOWED_DEPS_$(1)); ver CLAUDE.md §5.2))
$$(foreach d,$(3),$$(if $$(filter $$(d),$$(ALLOWED_DEPS_$(1))),,$$(error lib '$(1)': dependência proibida em '$$(d)'. Permitidas: [$$(ALLOWED_DEPS_$(1))] (CLAUDE.md §5.2))))
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

# $(call oma_test,<nome>,<fontes>,<libs do projeto>)
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
include tests/base/module.mk

# ----------------------------------------------------------------------- regras

$(BUILD_DIR)/obj/%.o: %.cpp $(MAKEFILE_LIST)
	$(call say,CXX,$<)
	@mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS_BASE) $(DEPFLAGS) $(SRCFLAGS_$<) -c $< -o $@

-include $(patsubst %.cpp,$(BUILD_DIR)/obj/%.d,$(ALL_SOURCES))

.PHONY: all libs tests test clean distclean compdb format format-check tidy fixtures help

all: libs tests compdb

libs: $(ALL_LIBS)

tests: $(ALL_TESTS)

# FILTER=<padrão> roda só os testes cujo nome contém o padrão (filtro do Cest).
# JUNIT_DIR=<dir> grava um relatório JUnit por binário (usado na CI).
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

# compile_commands.json na raiz, para clangd e clang-tidy (flags do BUILD atual).
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

help:
	@echo 'Alvos:'
	@echo '  all           libs + testes + compile_commands.json (padrão)'
	@echo '  test          compila e roda os testes  [FILTER=padrão] [JUNIT_DIR=dir]'
	@echo '  libs | tests  só compila'
	@echo '  compdb        gera compile_commands.json para clangd/clang-tidy'
	@echo '  format        aplica clang-format    | format-check  verifica sem alterar'
	@echo '  tidy          roda clang-tidy nas libs'
	@echo '  fixtures      gera mídia de teste em tests/fixtures/generated (precisa de ffmpeg)'
	@echo '  clean         apaga build/$$BUILD    | distclean     apaga build/ inteiro'
	@echo 'Variáveis: BUILD=debug|release|asan|tsan  CXX=g++|clang++  WERROR=1|0  V=1 (verboso)'
