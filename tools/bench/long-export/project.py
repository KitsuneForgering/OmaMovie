"""The long-export edit: five-minute cuts of a ten-minute source, reordered, a dissolve between
cuts 1 and 2 and a sepia look on cut 3 and the last cut (at least five cuts), as an OTIO timeline (`otio`) and then patched into the converted
OmaMovie project (`looks`). The plan (`plan`) is what check.py expects frame by frame.
Usage: python3 -I project.py otio|looks|plan MINUTES SOURCE_OR_PROJECT [OUT]"""
import json
import sys

FPS = 30
SEGMENT = 300 * FPS  # frames per cut
DISSOLVE = 15  # frames each side of the cut between cuts 1 and 2


def plan(minutes):
    """(timeline start, source start) per cut, in frames; starts stay clear of both ends."""
    n = max(5, minutes // 5)
    cuts = []
    for k in range(n):
        source_s = 1 + (k * 37) % 298
        cuts.append({"at": k * SEGMENT, "from": source_s * FPS, "look": k in (3, n - 1)})
    return {"fps": FPS, "segment": SEGMENT, "dissolve_after": 1, "dissolve": DISSOLVE, "cuts": cuts}


def time(frames):
    return {"OTIO_SCHEMA": "RationalTime.1", "rate": FPS, "value": frames}


def otio(minutes, source):
    items = []
    p = plan(minutes)
    for k, cut in enumerate(p["cuts"]):
        if k == p["dissolve_after"] + 1:
            items.append({"OTIO_SCHEMA": "Transition.1", "name": "dissolve",
                          "transition_type": "SMPTE_Dissolve",
                          "in_offset": time(DISSOLVE), "out_offset": time(DISSOLVE)})
        items.append({"OTIO_SCHEMA": "Clip.2", "name": f"cut {k}",
                      "source_range": {"OTIO_SCHEMA": "TimeRange.1", "start_time": time(cut["from"]),
                                       "duration": time(SEGMENT)},
                      "media_references": {"DEFAULT_MEDIA": {
                          "OTIO_SCHEMA": "ExternalReference.1", "target_url": source}},
                      "active_media_reference_key": "DEFAULT_MEDIA"})
    track = {"OTIO_SCHEMA": "Track.1", "kind": "Video", "children": items}
    return {"OTIO_SCHEMA": "Timeline.1", "name": "long export",
            "tracks": {"OTIO_SCHEMA": "Stack.1", "children": [track]}}


def looks(minutes, project):
    doc = json.load(open(project))
    clips = doc["sequence"]["tracks"][0]["clips"]
    for k, cut in enumerate(plan(minutes)["cuts"]):
        if cut["look"]:
            video = clips[k].setdefault("video", {})
            video["effects"] = [{"definition": "oma.look.sepia", "enabled": True,
                                 "params": {"amount": 1.0}}]
    return doc


if __name__ == "__main__":
    mode, minutes, path = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    out = {"otio": lambda: otio(minutes, path), "looks": lambda: looks(minutes, path),
           "plan": lambda: plan(minutes)}[mode]()
    json.dump(out, open(sys.argv[4], "w") if len(sys.argv) > 4 else sys.stdout)
