"""Independent check of the long export with FFmpeg only: plain cuts show the planned source
frame at their start, middle and end, the looked cuts differ from it, and the dissolve's
middle frame lies between its two clips. Frames are compared as 160x90 RGB thumbnails
(the source's hue turns once in ten minutes, so distant source times differ in colour).
Usage: python3 -I check.py OUTPUT SOURCE PLAN.json"""
import json
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
