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
	$(Q)ASAN_OPTIONS=alloc_dealloc_mismatch=0 build/fuzz/fuzz_otio -runs=$(FUZZ_RUNS) -max_len=65536 \
		build/fuzz/otio-corpus tests/fuzz/corpus/otio

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
