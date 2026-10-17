"""Independent check of the long export with FFmpeg only: plain cuts show the planned source
frame at their start, middle and end, the looked cuts differ from it, and the dissolve's
middle frame lies between its two clips. Frames are compared as 160x90 RGB thumbnails
(the source's hue turns once in ten minutes, so distant source times differ in colour).
Usage: python3 -I check.py OUTPUT SOURCE PLAN.json"""
import array
import json
import math
import subprocess
import sys

W, H = 160, 90


def frames(path, numbers):
    """RGB thumbnails of the given frame numbers, decoded in one pass."""
    wanted = sorted(set(numbers))
    expr = "+".join(f"eq(n\\,{n})" for n in wanted)
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-vf",
                          f"select='{expr}',scale={W}:{H}:flags=area,format=rgb24",
                          "-fps_mode", "passthrough", "-f", "rawvideo", "-"],
                         capture_output=True, check=True).stdout
    size = W * H * 3
    return {n: raw[i * size:(i + 1) * size] for i, n in enumerate(wanted)}


def diff(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) / len(a)


def probe(path):
    result = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-show_streams",
                             "-of", "json", path], capture_output=True, check=True)
    data = json.loads(result.stdout)["streams"]
    return (next(s for s in data if s["codec_type"] == "video"),
            next(s for s in data if s["codec_type"] == "audio"))


def audio_rms(path, at):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-ss", str(at), "-t", "1", "-i", path,
                          "-map", "0:a:0", "-ac", "1", "-ar", "48000", "-f", "s16le", "-"],
                         capture_output=True, check=True).stdout
    samples = array.array("h")
    samples.frombytes(raw)
    if len(samples) < 45000:
        raise ValueError(f"audio at {at}s: only {len(samples)} samples")
    return math.sqrt(sum(x * x for x in samples) / len(samples))


output, source, plan = sys.argv[1], sys.argv[2], json.load(open(sys.argv[3]))
seg, cut_k, half = plan["segment"], plan["dissolve_after"] + 1, plan["dissolve"]
pairs = []  # (output frame, source frame, cut)
for k, cut in enumerate(plan["cuts"]):
    for offset in (30, seg // 2, seg - 30):
        pairs.append((cut["at"] + offset, cut["from"] + offset, k))
mid = plan["cuts"][cut_k]["at"]  # the dissolve's centre: outgoing tail and incoming head
a_src = plan["cuts"][cut_k - 1]["from"] + seg
b_src = plan["cuts"][cut_k]["from"]
out = frames(output, [p[0] for p in pairs] + [mid])
src = frames(source, [p[1] for p in pairs] + [a_src, b_src])
failures, worst = [], 0.0
video, audio = probe(output)
expected_frames = plan["cuts"][-1]["at"] + seg
actual_frames = int(video["nb_read_frames"])
expected_seconds = expected_frames / 30
if actual_frames != expected_frames:
    failures.append(f"frames {actual_frames}/{expected_frames}")
if abs(float(video["duration"]) - expected_seconds) > 0.05:
    failures.append(f"video duration {video['duration']} vs {expected_seconds:.3f}s")
if abs(float(audio["duration"]) - expected_seconds) > 0.1:
    failures.append(f"audio duration {audio['duration']} vs {expected_seconds:.3f}s")
for at in (1, expected_seconds / 2, expected_seconds - 2):
    level = audio_rms(output, at)
    if level < 100:
        failures.append(f"audio at {at:.1f}s is silent (RMS {level:.1f})")
for o, s, k in pairs:
    d = diff(out[o], src[s])
    if plan["cuts"][k]["look"]:
        if d < 10:
            failures.append(f"cut {k} frame {o}: look not visible ({d:.1f})")
    else:
        worst = max(worst, d)
        if d > 6:
            failures.append(f"cut {k} frame {o}: not source frame {s} ({d:.1f})")
average = bytes((x + y) // 2 for x, y in zip(src[a_src], src[b_src]))
d_mix, d_a, d_b = diff(out[mid], average), diff(out[mid], src[a_src]), diff(out[mid], src[b_src])
# Half-way, the mix sits near the average and about half-way from either end; the ends must
# differ enough for that to mean something.
if diff(src[a_src], src[b_src]) < 4 or not d_mix < 0.5 * min(d_a, d_b):
    failures.append(f"dissolve middle {mid} is not a mix ({d_mix:.1f} vs {d_a:.1f}, {d_b:.1f})")
print(f"checked {len(pairs)} frames: plain worst {worst:.2f} levels; dissolve mix {d_mix:.1f} "
      f"vs ends {d_a:.1f}/{d_b:.1f}; {'PASS' if not failures else 'FAIL'}")
for f in failures:
    print("  " + f)
sys.exit(1 if failures else 0)
