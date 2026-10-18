# Contributing to OmaMovie

Repository content, code comments, logs and commit messages are written in English.
Keep changes small enough to build and verify independently. Explain behavioral and
architectural consequences; update the relevant plan, ADR or evidence record when they
change. A build alone is not a correctness check.

## Build and checks

The `Makefile` is the front end to CMake (ADR-0018). `make help` is the current command
list; `PKGBUILD` is the dependency source of truth. Common commands:

```sh
make -j
make -j test
make -j BUILD=asan test
make -j BUILD=tsan test
make format-check
make tidy
make run-gui
make run-gui RUN_GUI_SMOKE=1
make fuzz
```

`make deps` installs the `PKGBUILD` dependencies on Arch/Omarchy and may use sudo.
`scripts/check-includes.sh` verifies dependency boundaries not expressed by CMake.
`scripts/package-check.sh` exercises the package in disposable containers. Benchmark
and spike commands, their fixtures and measurement protocols live beside each task in
`docs/Research/evidence/`, `docs/spikes/` and `tools/bench/`; run only the relevant ones.

Commit messages use Conventional Commits in English, with an imperative description
under 100 characters. Check a range with `scripts/check-commits.sh`. Do not create a
release tag as part of normal implementation work; the [release plan](docs/implementation-plan-m10-validation.md)
and CI define its separate gates.

## Change protocol

1. Read the relevant implementation milestone, ADR and current code/tests. A document's
   target architecture is not proof of implementation.
2. For a behavior change, run focused checks and the affected integration/workflow test.
   For concurrent code, include the appropriate sanitizer subset. For performance changes,
   record a same-fixture baseline, hypothesis, measurement and regression check.
3. Keep the supported hardware/format statement no broader than the validated evidence.
   Record fallback paths and reasons. Treat media and project files as untrusted input.
4. Update an ADR for a changed architectural decision. Keep observed results in evidence
   files rather than copying numbers into the agent guide.
5. Report what ran and what remains unverified. Do not silently ignore a relevant warning
   or failure.

Key sources: [implementation plan](docs/implementation-plan.md), [ADR index](docs/adr/README.md),
[UI design](docs/ui-design.md), [format architecture](docs/format-architecture.md),
[interop support levels](docs/interop.md), and [QA plan](docs/implementation-plan-qa.md).
