#!/bin/sh
# Runs a Cest test binary. `make test FILTER=pattern` exports OMA_TEST_FILTER through CTest; it is
# appended as the Cest name filter. Everything else (sanitizer environment, JUnit, working dir) is
# handled by CTest.
# shellcheck disable=SC2086
exec "$@" ${OMA_TEST_FILTER:-}
