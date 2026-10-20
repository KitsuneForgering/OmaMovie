"""Runs a command and prints its wall time, CPU time, peak RSS and DRM GPU use on stderr.

Usage: python3 -I measure.py COMMAND [ARGS...]
GNU time is not part of a default Arch install; this keeps the bench free of it.
GPU figures come from the kernel's DRM fdinfo (drm-usage-stats) of the command and its
descendants, sampled every 50 ms and counted once per DRM client: peak resident memory and
busy time per engine class at the last sample, so up to one interval of work at exit is missed.
Drivers without fdinfo stats report gpu=n/a.
"""
import os
import resource
import subprocess
import sys
import time


def descendants(root):
    children = {}
    for pid in filter(str.isdigit, os.listdir("/proc")):
        try:
            with open(f"/proc/{pid}/stat") as f:
                ppid = int(f.read().rsplit(")", 1)[1].split()[1])
        except (OSError, IndexError, ValueError):
            continue
        children.setdefault(ppid, []).append(int(pid))
    found, todo = [], [root]
    while todo:
        pid = todo.pop()
        found.append(pid)
        todo.extend(children.get(pid, []))
    return found


def drm_clients(pids):
    clients = {}  # client id -> {key: value}
    for pid in pids:
        try:
            fds = os.listdir(f"/proc/{pid}/fdinfo")
        except OSError:
            continue
        for fd in fds:
            try:
                with open(f"/proc/{pid}/fdinfo/{fd}") as f:
                    fields = dict(line.split(":", 1) for line in f if ":" in line)
            except OSError:
                continue
            if "drm-client-id" in fields:
                clients[fields["drm-client-id"].strip()] = fields
    return clients


def number(value):  # "123 ns", "4096 KiB", "7"
    parts = value.split()
    scale = {"KiB": 1024, "MiB": 1024**2, "GiB": 1024**3}.get(parts[1] if len(parts) > 1 else "", 1)
    return int(parts[0]) * scale


start = time.monotonic()
child = subprocess.Popen(sys.argv[1:])
peak_resident, engines, seen = 0, {}, False
while child.poll() is None:
    clients = drm_clients(descendants(child.pid))
    seen = seen or bool(clients)
    resident = 0
    for client_id, fields in clients.items():
        for key, value in fields.items():
            if key.startswith("drm-resident-"):
                resident += number(value)
            elif key.startswith("drm-engine-") and not key.startswith("drm-engine-capacity"):
                engines[(client_id, key[11:])] = number(value)
    peak_resident = max(peak_resident, resident)
    time.sleep(0.05)
code = child.returncode
wall = time.monotonic() - start
usage = resource.getrusage(resource.RUSAGE_CHILDREN)
busy = {}
for (_, engine), ns in engines.items():
    busy[engine] = busy.get(engine, 0) + ns
gpu = (f"gpu_peak_resident_mb={peak_resident / 1024**2:.0f} " +
       " ".join(f"gpu_{e}_s={ns / 1e9:.2f}" for e, ns in sorted(busy.items()))) if seen else "gpu=n/a"
print(
    f"measure: rc={code} wall_s={wall:.2f} user_s={usage.ru_utime:.2f} "
    f"sys_s={usage.ru_stime:.2f} peak_rss_mb={usage.ru_maxrss / 1024:.0f} {gpu}",
    file=sys.stderr,
)
sys.exit(code)
