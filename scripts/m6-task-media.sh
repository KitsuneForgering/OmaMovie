#!/usr/bin/env bash
# Generates the media for the M6 manual task sessions (Docs/m6-walkthrough.md) into a folder
# outside the repository. Everything comes from lavfi: no copyrighted media.
# Usage: scripts/m6-task-media.sh [directory]   (default: $XDG_VIDEOS_DIR or ~/Videos/omamovie-m6)
set -euo pipefail

out="${1:-${XDG_VIDEOS_DIR:-$HOME/Videos}/omamovie-m6}"
mkdir -p "$out"
ff() { ffmpeg -hide_banner -loglevel error -y "$@"; }
h264=(-c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac)

# Two "speakers" for the dialogue: distinct pictures and pitches, 20 s each, with a short
# beep every 2 s so cuts and slips can be judged by ear.
speaker() { # name, video source, tone Hz
    ff -f lavfi -i "$2" \
        -f lavfi -i "aevalsrc='0.3*sin(2*PI*$3*t)+0.4*lt(mod(t\\,2)\\,0.08)*sin(2*PI*1500*t)':s=48000:d=20" \
        "${h264[@]}" -shortest "$out/$1.mp4"
}
speaker dialogue-a "testsrc2=size=1920x1080:rate=30:duration=20" 220
speaker dialogue-b "smptehdbars=size=1920x1080:rate=30:duration=20" 330

# A silent cutaway and a portrait phone clip.
ff -f lavfi -i "mandelbrot=size=1920x1080:rate=30" -t 8 \
    -c:v libx264 -preset veryfast -pix_fmt yuv420p "$out/cutaway.mp4"
ff -f lavfi -i "testsrc=size=1080x1920:rate=30:duration=6" -f lavfi -i "sine=frequency=550:d=6" \
    "${h264[@]}" -shortest "$out/phone-clip.mp4"

# A long screen-recording-like take: 10 minutes at 1080p30 with a counter and a beep a minute.
ff -f lavfi -i "testsrc2=size=1920x1080:rate=30:duration=600" \
    -f lavfi -i "aevalsrc='0.4*lt(mod(t\\,60)\\,0.2)*sin(2*PI*1000*t)':s=48000:d=600" \
    "${h264[@]}" -crf 32 -shortest "$out/long-recording.mp4"

echo "M6 task media in $out"
