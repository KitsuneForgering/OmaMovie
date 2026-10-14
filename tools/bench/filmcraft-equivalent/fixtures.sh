# Fixtures with FilmCraft's own recipes (crates/ui-egui/examples/bench/fixtures.rs): testsrc2
# with grain at 24000/1001 fps, so the workload is the same one FilmCraft measures.
# Sourced by run.sh.

fixture() { # name size seconds audio(0|1) encoder-args...
    local dir=$1 name=$2 size=$3 secs=$4 audio=$5
    shift 5
    [[ -s $dir/$name ]] && return
    local src="testsrc2=s=$size:r=24000/1001:d=$secs,noise=alls=6:allf=t" faststart=(-movflags +faststart)
    [[ $name == *.webm ]] && faststart=()
    local in=(-f lavfi -i "$src")
    ((audio)) && in+=(-f lavfi -i "sine=f=440:d=$secs" -c:a aac -shortest)
    SVT_LOG=1 ffmpeg -v error -y "${in[@]}" "$@" "${faststart[@]}" "$dir/.tmp.$name" && mv "$dir/.tmp.$name" "$dir/$name"
}

make_fixtures() {
    local d=$1
    local x264=(-c:v libx264 -preset fast -crf 20 -pix_fmt yuv420p)
    local x265=(-c:v libx265 -preset fast -crf 22 -pix_fmt yuv420p -tag:v hvc1 -x265-params log-level=error)
    local x265_10=(-c:v libx265 -preset fast -crf 22 -profile:v main10 -pix_fmt yuv420p10le -tag:v hvc1
                   -x265-params log-level=error)
    local vp9=(-c:v libvpx-vp9 -deadline realtime -cpu-used 8 -b:v 0 -crf 32 -row-mt 1 -pix_fmt yuv420p)
    local av1=(-c:v libsvtav1 -preset 10 -crf 35 -pix_fmt yuv420p -svtav1-params keyint=48)
    local prores=(-c:v prores_ks -profile:v 3 -pix_fmt yuv422p10le -vendor apl0)
    fixture "$d" a1080.mp4 1920x1080 20 1 "${x264[@]}"
    fixture "$d" a2160.mp4 3840x2160 10 0 "${x264[@]}"
    fixture "$d" hevc2160.mp4 3840x2160 10 0 "${x265[@]}"
    fixture "$d" vp92160.webm 3840x2160 10 0 "${vp9[@]}"
    fixture "$d" av12160.mp4 3840x2160 10 0 "${av1[@]}"
    fixture "$d" dec_h264_1080.mp4 1920x1080 5 0 "${x264[@]}"
    fixture "$d" dec_h264_2160.mp4 3840x2160 3 0 "${x264[@]}"
    fixture "$d" dec_hevc_1080.mp4 1920x1080 5 0 "${x265[@]}"
    fixture "$d" dec_hevc_2160.mp4 3840x2160 3 0 "${x265[@]}"
    fixture "$d" dec_hevc10_2160.mp4 3840x2160 3 0 "${x265_10[@]}"
    fixture "$d" dec_vp9_1080.webm 1920x1080 5 0 "${vp9[@]}"
    fixture "$d" dec_vp9_2160.webm 3840x2160 3 0 "${vp9[@]}"
    fixture "$d" dec_av1_1080.mp4 1920x1080 5 0 "${av1[@]}"
    fixture "$d" dec_av1_2160.mp4 3840x2160 3 0 "${av1[@]}"
    fixture "$d" dec_prores_1080.mov 1920x1080 2 0 "${prores[@]}"
    fixture "$d" dec_prores_2160.mov 3840x2160 1 0 "${prores[@]}"
}
