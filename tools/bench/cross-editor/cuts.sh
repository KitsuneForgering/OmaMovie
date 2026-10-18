# The shared edit: five cuts of the source, reordered, one of them half a second long.
# Both editors get the same frame ranges: OmaMovie through an OTIO timeline converted by
# oma-project (the app's own import), melt through a playlist of in/out points. Sourced by run.sh.

# start:frames pairs, in source frames at 24000/1001.
kCuts=(400:40 24:72 200:60 0:12 300:72)

cuts_total() {
    local c n=0
    for c in "${kCuts[@]}"; do n=$((n + ${c#*:})); done
    echo "$n"
}

# cuts_project source out.omamovie: the edit as an OmaMovie project.
cuts_project() {
    local src=$1 out=$2 otio c items=""
    otio=$(mktemp --suffix=.otio)
    for c in "${kCuts[@]}"; do
        items+="${items:+,}$(printf '{"OTIO_SCHEMA":"Clip.2","name":"cut","source_range":{"OTIO_SCHEMA":"TimeRange.1","start_time":{"OTIO_SCHEMA":"RationalTime.1","rate":23.976023976023978,"value":%d},"duration":{"OTIO_SCHEMA":"RationalTime.1","rate":23.976023976023978,"value":%d}},"media_references":{"DEFAULT_MEDIA":{"OTIO_SCHEMA":"ExternalReference.1","target_url":"%s"}},"active_media_reference_key":"DEFAULT_MEDIA"}' \
            "${c%:*}" "${c#*:}" "$src")"
    done
    printf '{"OTIO_SCHEMA":"Timeline.1","name":"cuts","tracks":{"OTIO_SCHEMA":"Stack.1","children":[{"OTIO_SCHEMA":"Track.1","kind":"Video","children":[%s]}]}}' \
        "$items" > "$otio"
    rm -f "$out"
    "$cli" convert "$otio" "$out" > /dev/null
    local rc=$?
    rm -f "$otio"
    return $rc
}

# The same edit as melt arguments (out is inclusive).
cuts_melt_args() {
    local src=$1 c
    for c in "${kCuts[@]}"; do
        printf '%s\n' "$src" "in=${c%:*}" "out=$((${c%:*} + ${c#*:} - 1))"
    done
}

# verify_cuts source out: every output frame against the source frame the edit puts there, in
# one pass (the expected movie is the source trimmed and joined the same way); the worst luma
# PSNR shows a wrong cut point or a duplicated/missing frame.
verify_cuts() {
    local src=$1 out=$2 c i=0 graph="" joins="" stats
    for c in "${kCuts[@]}"; do
        graph+="[1:v]trim=start_frame=${c%:*}:end_frame=$((${c%:*} + ${c#*:})),setpts=PTS-STARTPTS[s$i];"
        joins+="[s$i]"
        i=$((i + 1))
    done
    stats=$(mktemp)
    ffmpeg -v error -i "$out" -i "$src" -lavfi \
        "${graph}${joins}concat=n=$i:v=1:a=0[e];[0:v]trim=end_frame=$(cuts_total),setpts=PTS-STARTPTS[o];[o][e]psnr=stats_file=$stats" \
        -f null - 2>/dev/null
    awk '{ for (f = 1; f <= NF; f++) if ($f ~ /^psnr_y:/) { split($f, v, ":"); y = v[2] == "inf" ? 99 : v[2];
           if (n == 0 || y < w) { w = y; at = $1 } n++ } }
         END { printf "cut frames compared=%d worst psnr_y:%s at %s\n", n, w, at }' "$stats"
    rm -f "$stats"
}
