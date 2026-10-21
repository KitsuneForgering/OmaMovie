# One export per editor and encoder route, all from the same source file. Sourced by run.sh
# ($app, $here set there). Settings follow each encoder's defaults in both editors: libx264
# CRF 23 preset medium, h264_vaapi constant QP; no B-frames (OmaMovie writes none); AAC 192k.
# Kdenlive renders through MLT's `melt`, so melt with Kdenlive's VA-API recipe is its engine.

# export_oma route(hardware|software) out input: a video or a project; a private config and
# cache per run (cold). With OMA_BENCH_HEADLESS=1, tools/bench/export-headless's export_bench
# runs the same exporter without a window (sessions that cannot present).
export_oma() {
    local route=$1 out=$2 src=$3 home rc=0
    if [[ ${OMA_BENCH_HEADLESS:-} == 1 ]]; then
        local args=()
        [[ $route == software ]] && args+=(software)
        python3 -I "$here/measure.py" timeout 900 "$repo/build/release/tools/export_bench" "$src" "$out" "${args[@]}"
        return
    fi
    home=$(mktemp -d)
    mkdir -p "$home/config/omamovie"
    printf '[export]\nencoder=%s\n' "$route" > "$home/config/omamovie/settings.ini"
    XDG_CONFIG_HOME="$home/config" XDG_CACHE_HOME="$home/cache" XDG_DATA_HOME="$home/data" \
        python3 -I "$here/measure.py" timeout 900 "$app" --export-audit "$src" "$out" || rc=$?
    rm -rf "$home"
    return "$rc"
}

# export_melt route out producer-args...: one file, or a playlist of file in= out= triples.
export_melt() {
    local route=$1 out=$2 video
    shift 2
    if [[ $route == hardware ]]; then
        video=(vaapi_device=/dev/dri/renderD128 vf=format=nv12,hwupload vcodec=h264_vaapi)
    else
        video=(vcodec=libx264 preset=medium crf=23)
    fi
    python3 -I "$here/measure.py" timeout 900 melt -quiet "$@" -consumer "avformat:$out" \
        "${video[@]}" bf=0 acodec=aac ab=192k real_time=-1
}

# The installed editor's version line, for the report.
editor_version() {
    case $1 in
        oma) echo "OmaMovie $(git -C "$here" rev-parse --short HEAD)$(git -C "$here" diff --quiet || echo ' (dirty)')$([[ ${OMA_BENCH_HEADLESS:-} == 1 ]] && echo ', headless export_bench')" ;;
        melt) echo "$(melt --version 2>&1 | head -1) (Kdenlive $(kdenlive --version 2>/dev/null | awk '{print $2}'))" ;;
    esac
}
