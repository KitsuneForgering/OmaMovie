# Session stress testing

The existing headless Session tests exercise rapid transport and edit intents, project
switches during playback and export, export cancellation, and destruction while busy.
It also runs repeated open, edit, play, save, export, and close cycles with file descriptor
and resident memory checks.
`tools/bench/stress/run.sh` repeats these tests with a recorded seed. It keeps each run's
output, wall time, CPU time, and peak process RSS in a directory under `/tmp` by default.
The test process must pass every case in every round. The runner stops on the first failure
and prints the log path and exit code; retain that seed to reproduce the failure.

```sh
make fixtures
cmake --build build/debug --target app_tests -j2
tools/bench/stress/run.sh 3

# Longer local soak, bounded to 2,000 transport and 200 project-switch intents per round:
OMA_STRESS_TRANSPORT_OPS=1000 OMA_STRESS_SWITCH_OPS=120 \
  tools/bench/stress/run.sh 10 /tmp/omamovie-soak

# Reproduce one seed from a failing log:
OMA_STRESS_SEED=9945 build/debug/tests/app_tests 'survives seeded'
```

`BUILD=asan` or `BUILD=tsan` selects the corresponding prebuilt `app_tests` binary. The runner
gives it ctest's sanitizer environment (`OMA_TEST_ENV`, with `tests/support/*.supp`); before
2026-10-22 it did not, and a TSan soak stopped on FFmpeg's suppressed thread hand-offs. Each case
has a 600-second timeout. These are correctness and lifetime stress tests, not a speed
benchmark; wall and RSS measurements help spot large regressions across repetitions, but
include setup and are affected by host load. The headless tests do not establish visible
frame pacing, A/V sync, GPU memory use, or performance on other GPU vendors. Use the
[professional workload spike](professional-workload.md) for bounded multi-source decode
and render timings, and the app's `--m4-audit` for presentation behavior.

The [first recorded three-round run](../Research/evidence/2026-10-14-session-stress.txt)
passed on Iris Xe. A sandbox run on llvmpipe exceeded the resource test's RSS allowance;
run resource checks on the intended graphics stack and keep the failing log if this occurs.
