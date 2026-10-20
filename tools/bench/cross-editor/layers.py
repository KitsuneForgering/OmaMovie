#!/usr/bin/env python3
"""The three-layer scene as an OmaMovie project: three 240-frame 1080p23.976 clips stacked on
V1-V3, V2 and V3 scaled by half into the top-left and bottom-right quadrants. melt gets the
same picture from affine transitions (layers.sh); OmaMovie does not import MLT compositing.
Usage: python3 -I layers.py OMA_PROJECT_CLI SOURCE_DIR OUT.omamovie"""
import json
import subprocess
import sys
import tempfile

cli, src, out = sys.argv[1:4]
rate = 24000 / 1001


def clip(name):
    time = lambda v: {"OTIO_SCHEMA": "RationalTime.1", "rate": rate, "value": v}
    return {"OTIO_SCHEMA": "Clip.2", "name": name,
            "source_range": {"OTIO_SCHEMA": "TimeRange.1", "start_time": time(0),
                             "duration": time(240)},
            "media_references": {"DEFAULT_MEDIA": {"OTIO_SCHEMA": "ExternalReference.1",
                                                   "target_url": f"{src}/{name}"}},
            "active_media_reference_key": "DEFAULT_MEDIA"}


tracks = [{"OTIO_SCHEMA": "Track.1", "name": f"V{i + 1}", "kind": "Video",
           "children": [clip(f"layer{i}.mp4")]} for i in range(3)]
with tempfile.TemporaryDirectory() as tmp:
    with open(f"{tmp}/layers.otio", "w") as f:
        json.dump({"OTIO_SCHEMA": "Timeline.1", "name": "layers",
                   "tracks": {"OTIO_SCHEMA": "Stack.1", "children": tracks}}, f)
    subprocess.run([cli, "convert", f"{tmp}/layers.otio", f"{tmp}/converted.omamovie"],
                   check=True, capture_output=True)
    with open(f"{tmp}/converted.omamovie") as f:
        project = json.load(f)
for track, (x, y) in zip(project["sequence"]["tracks"][1:], ((-480, -270), (480, 270))):
    track["clips"][0]["video"]["transform"].update(offset_x=x, offset_y=y,
                                                   scale_x=0.5, scale_y=0.5)
with open(out, "w") as f:
    json.dump(project, f, indent=1)
