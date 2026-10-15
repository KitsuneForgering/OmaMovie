"""Runs a command and prints its wall time, CPU time and peak RSS on stderr.

Usage: python3 -I measure.py COMMAND [ARGS...]
GNU time is not part of a default Arch install; this keeps the bench free of it.
"""
import resource
import subprocess
import sys
import time

start = time.monotonic()
code = subprocess.call(sys.argv[1:])
wall = time.monotonic() - start
usage = resource.getrusage(resource.RUSAGE_CHILDREN)
print(
    f"measure: rc={code} wall_s={wall:.2f} user_s={usage.ru_utime:.2f} "
    f"sys_s={usage.ru_stime:.2f} peak_rss_mb={usage.ru_maxrss / 1024:.0f}",
    file=sys.stderr,
)
sys.exit(code)
