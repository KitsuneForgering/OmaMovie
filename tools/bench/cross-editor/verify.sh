# Independent output checks with FFmpeg, never with the editor that wrote the file.
# Sourced by run.sh. A wrong render is a failure, not a fast result.

# verify source out: frame count, durations and luma PSNR against the source, one line.
verify() {
    local src=$1 out=$2 want got vdur adur size psnr
    want=$(frames_of "$src")
    got=$(frames_of "$out")
    vdur=$(ffprobe -v error -select_streams v:0 -show_entries stream=duration -of csv=p=0 "$out")
    adur=$(ffprobe -v error -select_streams a:0 -show_entries stream=duration -of csv=p=0 "$out")
    size=$(stat -c %s "$out")
    # Frame-aligned from the first frame; both inputs are BT.709 limited, so YUV compares directly.
    psnr=$(ffmpeg -v info -i "$out" -i "$src" -lavfi \
        "[0:v]trim=end_frame=$want,setpts=PTS-STARTPTS[a];[1:v]setpts=PTS-STARTPTS[b];[a][b]psnr" \
        -f null - 2>&1 | grep -o 'y:[0-9.]*' | tail -1)
    local verdict=pass
    [[ $got == "$want" ]] || verdict="FAIL(frames $got != $want)"
    awk -v v="$vdur" -v a="$adur" 'BEGIN { exit !(v - a < 0.05 && a - v < 0.05) }' ||
        verdict="$verdict, A/V length differs"
    echo "frames=$got/$want video_s=$vdur audio_s=$adur bytes=$size psnr_$psnr verdict=$verdict"
}

frames_of() {
    ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames \
        -of csv=p=0 "$1"
}

# verify_frames out want: frame count and A/V length only (for edits with their own frame check).
verify_frames() {
    local got vdur adur verdict=pass
    got=$(frames_of "$1")
    vdur=$(ffprobe -v error -select_streams v:0 -show_entries stream=duration -of csv=p=0 "$1")
    adur=$(ffprobe -v error -select_streams a:0 -show_entries stream=duration -of csv=p=0 "$1")
    [[ $got == "$2" ]] || verdict="FAIL(frames $got != $2)"
    echo "frames=$got/$2 video_s=$vdur audio_s=$adur bytes=$(stat -c %s "$1") verdict=$verdict"
}
