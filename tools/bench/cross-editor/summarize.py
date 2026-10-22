#!/usr/bin/env python3
"""Medians and ranges per editor and route from cross-editor reports (run.sh, layers.sh): the
table Docs/analysis/omamovie-2026-10-20-cross-editor-summary.md was built by hand from.
Failed exports are counted, never folded into the figures.
Usage: python3 -I summarize.py REPORT..."""
import re
import statistics
import sys

ROUND = re.compile(r"^round (\d+) (\w+) (\w+):")
FIELD = re.compile(r"([a-z_]+)=([-0-9.]+)")
COLUMNS = ("wall_s", "user_s", "peak_rss_mb", "worst_psnr_y", "mean_psnr_u", "mean_psnr_v",
           "psnr_vs_reference")


def runs(path):
    current = None
    for line in open(path, encoding="utf-8"):
        match = ROUND.match(line)
        if match:
            if current:
                yield current
            current = {"case": (match[2], match[3]), "passed": "rc=0" in line}
            current.update((k, float(v)) for k, v in FIELD.findall(line))
            if "psnr_vs_reference" in current:  # layers.sh checks on the same line
                current["passed"] = True
        elif current and line.startswith(("PASS", "FAIL")):
            current["passed"] = current["passed"] and line.startswith("PASS")
            current.update((k, float(v)) for k, v in FIELD.findall(line))
    if current:
        yield current


def main(paths):
    if not paths:
        raise SystemExit(__doc__)
    for path in paths:
        print(f"== {path}")
        for line in open(path, encoding="utf-8"):
            if line.startswith(("editors:", "scene:")):
                print("   " + line.strip())
        cases = {}
        for run in runs(path):
            cases.setdefault(run["case"], []).append(run)
        for (editor, route), group in sorted(cases.items()):
            good = [r for r in group if r["passed"]]
            cells = []
            for column in COLUMNS:
                values = [r[column] for r in good if column in r]
                if values:
                    cells.append(f"{column} {statistics.median(values):g} "
                                 f"[{min(values):g}-{max(values):g}]")
            failed = len(group) - len(good)
            print(f"   {editor} {route}: {len(good)} passed"
                  f"{f', {failed} FAILED' if failed else ''}; " + "; ".join(cells))


if __name__ == "__main__":
    main(sys.argv[1:])
