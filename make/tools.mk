# Parser fuzzing (libFuzzer, Clang): build with the fuzz preset, then run each target from its
# seed corpus in tests/fuzz/corpus. New inputs land in build/fuzz/, never in the repository.
# `make fuzz-<target>` builds and runs one alone (CI runs them as parallel jobs).
FUZZ_TARGETS := cube project subtitles otio edl mlt fcpxml probe
# simdjson's inline padded_string pairs new(std::nothrow)[] with delete[]; under clang's
# sanitizers with libstdc++ the delete reaches free(), a toolchain mismatch outside our code.
# libstdc++'s stable_sort buffer meets the same mismatch in the subtitle and OTIO parsers.
FUZZ_ENV_project   := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_subtitles := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_otio      := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_edl       := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_mlt       := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_fcpxml    := ASAN_OPTIONS=alloc_dealloc_mismatch=0
FUZZ_ENV_probe     := ASAN_OPTIONS=alloc_dealloc_mismatch=0
# Each probe input runs FFmpeg's demuxers (~125 runs/s): bounded by time as well as runs.
FUZZ_ARGS_probe    := -timeout=10 -max_total_time=$(FUZZ_PROBE_SECONDS)

fuzz: $(addprefix fuzz-,$(FUZZ_TARGETS))

fuzz-%:
	$(Q)cmake --preset fuzz
	$(Q)cmake --build --preset fuzz --target fuzz_$*
	$(Q)$(FUZZ_ENV_$*) build/fuzz/fuzz_$* -runs=$(FUZZ_RUNS) -max_len=65536 $(FUZZ_ARGS_$*) \
		build/fuzz/$*-corpus tests/fuzz/corpus/$*

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
	@echo '  fuzz          run the parser fuzz targets with clang (libFuzzer) [FUZZ_RUNS=n FUZZ_PROBE_SECONDS=s]'
	@echo '  fuzz-<name>   one of them: cube project subtitles otio edl mlt fcpxml probe'
	@echo '  spikes        build the M1 spikes in tools/spikes (Vulkan + FFmpeg + Qt Quick/RHI)'
	@echo '  install       install the app tree [DESTDIR=dir PREFIX=/usr/local] (BUILD=release)'
	@echo '  run-gui       open the Qt editor shell [GUI_FILE=path; RUN_GUI_SMOKE=1]'
	@echo '  deps          install the dependencies declared in the PKGBUILD (uses sudo pacman)'
	@echo '  clean         remove build/$$BUILD   | distclean     remove all of build/'
	@echo 'Variables: BUILD=debug|release|asan|tsan  CXX=g++|clang++  WERROR=1|0  V=1 (verbose)'
