# Decoder throughput, the section FilmCraft calls `decode`. Sourced by run.sh.

# Every frame through FFmpeg (the decoder oma-media wraps), software and VA-API surfaces only:
# not the app's import, composite or presentation. CPU ms/frame median of 3, fps best of 3.
section_decode() {
    echo; echo "== decode (FFmpeg; VA-API = to the surface only)"
    local f mode n args
    for f in "$1"/dec_*; do
        n=$(ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 "$f")
        for mode in sw vaapi; do
            args=(-i "$f")
            if [[ $mode == vaapi ]]; then
                # FFmpeg falls back to software silently (S1): measure only a negotiated surface.
                ffmpeg -hide_banner -loglevel debug -hwaccel vaapi -hwaccel_output_format vaapi -i "$f" -map 0:v \
                    -frames:v 3 -f null - 2>&1 | grep -q 'Format vaapi chosen by get_format' ||
                    { printf '%-22s %-6s no VA-API decode\n' "$(basename "$f")" "$mode"; continue; }
                args=(-hwaccel vaapi -hwaccel_output_format vaapi -i "$f")
            fi
            for _ in 1 2 3; do
                ffmpeg -hide_banner -nostats -benchmark "${args[@]}" -map 0:v -f null - 2>&1 |
                    grep -oE 'utime=[0-9.]+s stime=[0-9.]+s rtime=[0-9.]+s' | tail -1 | tr -d 'a-z=' | tr 's' ' '
            done | awk -v n="$n" -v f="$(basename "$f")" -v m="$mode" '
                NF == 3 { c[++k] = ($1 + $2) * 1000 / n; if (!b || $3 < b) b = $3 }
                END { if (!k) { printf "%-22s %-6s failed or no VA-API\n", f, m; exit }
                      asort(c); printf "%-22s %-6s CPU %7.1f ms/frame %6.0f fps\n", f, m, c[int((k + 1) / 2)], n / b }'
        done
    done
}
