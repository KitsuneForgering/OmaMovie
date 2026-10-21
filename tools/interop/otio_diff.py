#!/usr/bin/env python3
"""Differential check of OmaMovie's OTIO import against the OpenTimelineIO library itself.

Random timelines (cuts, gaps, centred dissolves, disabled clips, video and audio tracks, NTSC
and integer rates)
are written by the OTIO library; `oma-project convert` turns each into an OmaMovie project, and
every clip's record start, length and source in must equal what OTIO computes
(range_in_parent, source_range), exactly, as fractions of a second.
With --mixed, later clips take random rates; positions must then agree within one sequence tick
per clip before them (lengths off the grid are rounded and reported).
Usage: otio_diff.py OMA_PROJECT_CLI [seeds=200] [--mixed]  (needs ffmpeg, the opentimelineio module)
"""

import json
import random
import subprocess
import sys
import tempfile
from fractions import Fraction
from pathlib import Path

import opentimelineio as otio

RATES = [24.0, 25.0, 30.0, 24000 / 1001, 30000 / 1001]


def exact(rate):
    """The fraction OTIO's float rate stands for (NTSC rates written as 24000/1001 floats)."""
    for num in (24000, 30000, 60000):
        if abs(rate - num / 1001) < 1e-9:
            return Fraction(num, 1001)
    return Fraction(rate).limit_denominator(1000)


def seconds(t):
    # Mixed rates give fractional frame values (3.125 s is 78.125 frames at 25 fps).
    return Fraction(t.value).limit_denominator(1_000_000) / exact(t.rate)


def timeline(rng, media, rate, mixed=False):
    tl = otio.schema.Timeline(name="diff")
    for v in range(rng.randint(1, 4)):
        audio = v > 0 and rng.random() < 0.4
        track = otio.schema.Track(name=f"T{v + 1}", kind=otio.schema.TrackKind.Audio if audio
                                  else otio.schema.TrackKind.Video)
        for i in range(rng.randint(1, 8)):
            if i and rng.random() < 0.25:
                track.append(otio.schema.Gap(source_range=otio.opentime.TimeRange(
                    duration=otio.opentime.RationalTime(rng.randint(1, 48), rate))))
                continue
            if i and isinstance(track[-1], otio.schema.Clip) and rng.random() < 0.3:
                half = rng.randint(1, 6)  # centred; both clips have handles (start >= 24)
                track.append(otio.schema.Transition(
                    transition_type=otio.schema.TransitionTypes.SMPTE_Dissolve,
                    in_offset=otio.opentime.RationalTime(half, rate),
                    out_offset=otio.opentime.RationalTime(half, rate)))
            ref = otio.schema.ExternalReference(target_url=media)
            start = rng.randint(24, 900)
            at = rng.choice(RATES) if mixed and i else rate  # the first clip sets the sequence
            clip = otio.schema.Clip(name=f"c{v}{i}", media_reference=ref,
                                    source_range=otio.opentime.TimeRange(
                                        otio.opentime.RationalTime(start, at),
                                        otio.opentime.RationalTime(rng.randint(13, 96), at)))
            clip.enabled = rng.random() > 0.1  # a disabled clip must leave a gap
            track.append(clip)
        if mixed:  # transitions between clips of different rates are not this check's subject
            for t in [x for x in track if isinstance(x, otio.schema.Transition)]:
                track.remove(t)
        if isinstance(track[-1], otio.schema.Transition):
            track.pop()
        tl.tracks.append(track)
    return tl


def expected(tl):
    """Per track kind, each enabled clip as (record start, length, source in), in seconds."""
    out = {"video": [], "audio": []}
    for track in tl.tracks:
        rows = []
        for item in track:
            if isinstance(item, otio.schema.Clip) and item.enabled:
                at = track.range_of_child(item)
                rows.append((seconds(at.start_time), seconds(at.duration),
                             seconds(item.source_range.start_time)))
        out["audio" if track.kind == otio.schema.TrackKind.Audio else "video"].append(rows)
    return out


def time_of(value):
    num, _, den = value["timebase"].partition("/")
    return Fraction(value["value"]) * Fraction(int(num), int(den or 1))


def actual(project):
    tracks = json.loads(project.read_text())["sequence"]["tracks"]
    return {kind: [[(time_of(c["start"]), time_of(c["duration"]), time_of(c["source_in"]))
                    for c in t["clips"]] for t in tracks if t["kind"] == kind]
            for kind in ("video", "audio")}


def close(want, got, tick):
    """Mixed rates: lengths off the sequence grid round to the nearest tick, so the k-th clip
    may sit up to k ticks from the library's exact position."""
    if got is None or want.keys() != got.keys():
        return False
    for kind in want:
        if len(want[kind]) != len(got[kind]):
            return False
        for a, b in zip(want[kind], got[kind]):
            if len(a) != len(b) or any(abs(x - y) > (k + 1) * tick for k, (p, q) in enumerate(zip(a, b))
                                       for x, y in zip(p, q)):
                return False
    return True


def main():
    cli = sys.argv[1]
    seeds = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    mixed = "--mixed" in sys.argv
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        # A minute of picture and sound: every generated source range lies inside it.
        media = str(Path(tmp) / "media.mp4")
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                        "testsrc2=s=160x90:r=24000/1001:d=60", "-f", "lavfi", "-i",
                        "sine=f=440:d=60:r=48000", "-c:v", "libx264", "-preset", "ultrafast",
                        "-c:a", "aac", "-shortest", media], check=True)
        for seed in range(1, seeds + 1):
            rng = random.Random(seed)
            rate = rng.choice(RATES)
            tl = timeline(rng, media, rate, mixed)
            source, project = Path(tmp) / "t.otio", Path(tmp) / "t.omamovie"
            otio.adapters.write_to_file(tl, str(source))
            project.unlink(missing_ok=True)
            run = subprocess.run([cli, "convert", str(source), str(project)],
                                 capture_output=True, text=True)
            want, got = expected(tl), actual(project) if project.exists() else None
            tick = (time_of({"value": 1, "timebase": json.loads(project.read_text())
                             ["sequence"]["timebase"]}) if project.exists() else 0)
            if (want != got) if not mixed else not close(want, got, tick):
                failures += 1
                print(f"FAIL seed {seed} rate {rate}: want {want}\n  got {got}\n  {run.stdout}")
    print(f"{seeds - failures}/{seeds} seeds agree with OpenTimelineIO {otio.__version__}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
