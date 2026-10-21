#!/usr/bin/env python3
"""Import cost by timeline size: one-track OTIO timelines of N cuts (written by the
OpenTimelineIO library) through `oma-project convert`, each run checked to have saved its
project. Usage: run.py OMA_PROJECT_CLI [sizes...]   (default 1000 2000 4000 8000 16000)
"""

import subprocess
import sys
import tempfile
import time
from pathlib import Path

import opentimelineio as otio


def main():
    cli = sys.argv[1]
    sizes = [int(n) for n in sys.argv[2:]] or [1000, 2000, 4000, 8000, 16000]
    with tempfile.TemporaryDirectory() as tmp:
        media = Path(tmp) / "m.mp4"
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=160x90:r=24:d=60",
                        "-f", "lavfi", "-i", "sine=d=60", "-c:v", "libx264", "-preset",
                        "ultrafast", "-c:a", "aac", "-shortest", str(media)], check=True)
        for n in sizes:
            tl, track = otio.schema.Timeline(), otio.schema.Track()
            for i in range(n):
                track.append(otio.schema.Clip(
                    media_reference=otio.schema.ExternalReference(target_url=str(media)),
                    source_range=otio.opentime.TimeRange(otio.opentime.RationalTime(i % 1000, 24),
                                                         otio.opentime.RationalTime(12, 24))))
            tl.tracks.append(track)
            source, project = Path(tmp) / f"n{n}.otio", Path(tmp) / f"n{n}.omamovie"
            otio.adapters.write_to_file(tl, str(source))
            start = time.perf_counter()
            run = subprocess.run([cli, "convert", str(source), str(project)],
                                 capture_output=True, text=True)
            took = time.perf_counter() - start
            if "saved:" not in run.stdout:  # convert refuses to overwrite: never time a refusal
                print(f"{n} clips: FAILED {run.stdout[-200:]}{run.stderr[-200:]}")
                return 1
            print(f"{n} clips: {took:.2f} s", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
