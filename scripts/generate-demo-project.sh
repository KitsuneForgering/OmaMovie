#!/bin/sh
# Rebuild the small, self-created footage and editable project shipped with OmaMovie.
set -eu

cd "$(dirname "$0")/.."
demo=apps/omamovie/demo
mkdir -p "$demo/media"

make oma-project
cli=build/debug/tools/oma-project/oma-project

# Rebuild from an empty demo set so stale clips cannot remain in a package.
rm -f "$demo/First Edit.omamovie" \
    "$demo/media/01-opening.mp4" \
    "$demo/media/02-detail.mp4" \
    "$demo/media/03-finale.mp4"

make_shot() {
    file=$1
    color=$2
    title=$3
    note=$4
    tone=$5
    ffmpeg -nostdin -hide_banner -loglevel error -y \
        -f lavfi -i "color=c=$color:s=640x360:r=30:d=4" \
        -f lavfi -i "color=c=white@0.25:s=72x72:r=30:d=4,format=rgba" \
        -f lavfi -i "sine=frequency=$tone:sample_rate=48000:duration=4" \
        -filter_complex "[0:v][1:v]overlay=x='40+110*t':y=86:shortest=1,drawbox=x=0:y=246:w=640:h=114:color=black@0.25:t=fill,drawtext=font=Sans:text='$title':fontcolor=white:fontsize=42:x=38:y=263,drawtext=font=Sans:text='$note':fontcolor=white:fontsize=20:x=40:y=318[v]" \
        -map '[v]' -map '2:a' \
        -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p \
        -c:a aac -b:a 96k -shortest "$demo/media/$file"
}

make_shot 01-opening.mp4 '#183d5b' OPENING 'Start with this shot' 440
make_shot 02-detail.mp4 '#315347' DETAIL 'Try trimming this cut' 554
make_shot 03-finale.mp4 '#704539' FINALE 'Add your own ending' 659

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
edl=$scratch/source.edl
project=$scratch/source.omamovie
cat > "$edl" <<EOF
TITLE: OmaMovie first edit
FCM: NON-DROP FRAME
001  AX       V     C        00:00:00:00 00:00:02:00 00:00:00:00 00:00:02:00
* SOURCE FILE: $PWD/$demo/media/01-opening.mp4
002  BX       V     C        00:00:00:00 00:00:02:00 00:00:02:00 00:00:04:00
* SOURCE FILE: $PWD/$demo/media/02-detail.mp4
003  CX       V     C        00:00:00:00 00:00:02:00 00:00:04:00 00:00:06:00
* SOURCE FILE: $PWD/$demo/media/03-finale.mp4
EOF
"$cli" convert "$edl" "$project" 30
# The released project resolves media beside itself, independently of the checkout path.
jq '(.media[]) |= (.path = ("media/" + .name) | .absolute_path = "")' \
    "$project" > "$demo/First Edit.omamovie"
"$cli" validate "$demo/First Edit.omamovie"
"$cli" inspect "$demo/First Edit.omamovie"
