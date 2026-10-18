#!/usr/bin/env python3
"""Independent, failing export oracle for the shared BT.709 scenes."""

import array
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

CUTS = [(400, 40), (24, 72), (200, 60), (0, 12), (300, 72)]
RATE = 24000 / 1001


def command(*args):
    return subprocess.run(args, check=True, capture_output=True).stdout


def streams(path):
    data = json.loads(command("ffprobe", "-v", "error", "-count_frames", "-show_streams",
                              "-of", "json", str(path)))
    video = next(s for s in data["streams"] if s["codec_type"] == "video")
    audio = next(s for s in data["streams"] if s["codec_type"] == "audio")
    return video, audio


def frame_count(video):
    return int(video["nb_read_frames"])


def expected_filter(scene, count):
    if scene == "whole":
        return f"[1:v]trim=end_frame={count},setpts=PTS-STARTPTS[e]"
    pieces = []
    labels = []
    for i, (start, length) in enumerate(CUTS):
        pieces.append(f"[1:v]trim=start_frame={start}:end_frame={start + length},"
                      f"setpts=PTS-STARTPTS[s{i}]")
        labels.append(f"[s{i}]")
    pieces.append("".join(labels) + f"concat=n={len(CUTS)}:v=1:a=0[e]")
    return ";".join(pieces)


def assert_picture(source, output, scene, count):
    with tempfile.TemporaryDirectory() as tmp:
        stats = Path(tmp) / "psnr.txt"
        graph = (expected_filter(scene, count) + ";"
                 f"[0:v]trim=end_frame={count},setpts=PTS-STARTPTS[o];"
                 f"[o][e]psnr=stats_file={stats}")
        command("ffmpeg", "-v", "error", "-i", str(output), "-i", str(source),
                "-filter_complex", graph, "-f", "null", "-")
        scores = []
        for line in stats.read_text().splitlines():
            score = next(part[7:] for part in line.split() if part.startswith("psnr_y:"))
            scores.append(float(score))
    if len(scores) != count:
        raise AssertionError(f"picture: compared {len(scores)} frames, expected {count}")
    worst = min(scores)
    if math.isnan(worst) or worst < 35:
        raise AssertionError(f"picture: worst luma PSNR {worst:.2f} dB, minimum 35 dB")
    return worst


def decoded_audio(path):
    pcm = command("ffmpeg", "-v", "error", "-i", str(path), "-map", "0:a:0",
                  "-ac", "1", "-ar", "48000", "-f", "s16le", "-")
    samples = array.array("h")
    samples.frombytes(pcm)
    return samples


def correlation(reference, actual, source_at, output_at):
    # A short seeded-noise signature has no repeating tone period. Search only within
    # 50 ms for encoder delay; a dropped/reordered cut cannot match another interval.
    length, stride = 3840, 8
    ref = reference[source_at:source_at + length:stride]
    if len(ref) != length // stride:
        raise AssertionError("audio: source checkpoint window is incomplete")
    ref_energy = sum(value * value for value in ref)
    best, best_lag, best_gain = -1.0, 0, 0.0
    for lag in range(-2400, 2401, 16):
        start = output_at + lag
        if start < 0:
            continue
        got = actual[start:start + length:stride]
        if len(got) != len(ref):
            continue
        energy = sum(value * value for value in got)
        if energy == 0:
            continue
        score = sum(x * y for x, y in zip(ref, got)) / math.sqrt(ref_energy * energy)
        if score > best:
            best, best_lag, best_gain = score, lag, math.sqrt(energy / ref_energy)
    return best, best_lag, best_gain


def assert_audio(source, output, scene, expected_seconds):
    samples = decoded_audio(output)
    original = decoded_audio(source)
    expected = round(expected_seconds * 48000)
    if abs(len(samples) - expected) > 4800:
        raise AssertionError(f"audio: {len(samples)} samples, expected {expected} ±4800")
    points = []
    if scene == "whole":
        points = [(fraction * expected_seconds, fraction * expected_seconds)
                  for fraction in (0.05, 0.5, 0.9)]
    else:
        at = 0
        for start, length in CUTS:
            for fraction in (0.25, 0.5, 0.75):
                offset = fraction * length
                points.append(((start + offset) / RATE, (at + offset) / RATE))
            at += length
    results = []
    for source_time, output_time in points:
        score, lag, gain = correlation(original, samples, round(source_time * 48000),
                                       round(output_time * 48000))
        if score < 0.4:
            raise AssertionError(f"audio at {output_time:.3f}s: wrong source interval "
                                 f"or drift (correlation {score:.3f})")
        if not 0.25 <= gain <= 4:
            raise AssertionError(f"audio at {output_time:.3f}s: relative level {gain:.2f}")
        results.append((score, lag, gain))
    return (min(score for score, _, _ in results),
            max(abs(lag) for _, lag, _ in results) / 48,
            min(gain for _, _, gain in results))


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("whole", "cuts"):
        raise SystemExit("usage: assert_output.py whole|cuts SOURCE OUTPUT")
    scene, source, output = sys.argv[1:]
    source_video, _ = streams(source)
    video, audio = streams(output)
    if frame_count(source_video) != 479 or (source_video["width"], source_video["height"]) != (1920, 1080):
        raise AssertionError("fixture: expected 479 decoded frames at 1920x1080")
    expected_tags = {"color_primaries": "bt709", "color_transfer": "bt709",
                     "color_space": "bt709", "color_range": "tv", "r_frame_rate": "24000/1001"}
    for name, value in expected_tags.items():
        if source_video.get(name) != value or video.get(name) != value:
            raise AssertionError(f"{name}: source={source_video.get(name)} "
                                 f"output={video.get(name)}, expected {value}")
    count = frame_count(source_video) if scene == "whole" else sum(n for _, n in CUTS)
    actual = frame_count(video)
    if actual != count:
        raise AssertionError(f"frames: {actual}, expected {count}")
    if video.get("width") != source_video.get("width") or video.get("height") != source_video.get("height"):
        raise AssertionError("picture: output dimensions differ from source")
    expected_seconds = count / RATE
    video_seconds = float(video["duration"])
    audio_seconds = float(audio["duration"])
    if abs(video_seconds - expected_seconds) > 1 / RATE + 0.01:
        raise AssertionError(f"video duration: {video_seconds:.3f}, expected {expected_seconds:.3f}")
    if abs(audio_seconds - expected_seconds) > 0.1:
        raise AssertionError(f"audio duration: {audio_seconds:.3f}, expected {expected_seconds:.3f}")
    worst = assert_picture(source, output, scene, count)
    audio_match, audio_lag_ms, audio_min_gain = assert_audio(source, output, scene, expected_seconds)
    print(f"PASS frames={count} duration={video_seconds:.3f}s worst_psnr_y={worst:.2f}dB "
          f"audio_min_corr={audio_match:.3f} max_lag={audio_lag_ms:.1f}ms "
          f"min_gain={audio_min_gain:.2f} "
          f"bytes={Path(output).stat().st_size}")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, StopIteration, KeyError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        sys.exit(1)
