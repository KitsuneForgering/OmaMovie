# Scenes: what each editor exports and how the output is checked. Sourced by run.sh after
# editors.sh, verify.sh and cuts.sh; sets oma_in (one path) and melt_in (melt producer args).
#   whole  the source exported as it is
#   cuts   five reordered cuts (cuts.sh), checked frame by frame against the source
#   speed  source frames 0-399 at 2x as Kdenlive saves it (an MLT timewarp): melt renders the
#          producer, OmaMovie imports the document melt writes for it (the app's MLT import)

scene_inputs() {
    local scene=$1 src=$2 dir=$3
    case $scene in
        whole)
            oma_in=$src
            # MLT's implicit producer end may round up by one frame. Bound both
            # editors to the same decoded source-frame interval explicitly.
            melt_in=("$src" "in=0" "out=$(($(frames_of "$src") - 1))")
            ;;
        cuts)
            oma_in="$dir/cuts.omamovie"
            cuts_project "$src" "$oma_in" || return 1
            mapfile -t melt_in < <(cuts_melt_args "$src")
            ;;
        speed)
            melt_in=("timewarp:2:$src" "in=0" "out=199")
            oma_in="$dir/speed.omamovie"
            melt -quiet "${melt_in[@]}" -consumer "xml:$dir/speed.mlt" &&
                "$cli" convert "$dir/speed.mlt" "$oma_in" > /dev/null || return 1
            ;;
        *) echo "unknown scene: $scene" >&2; return 1 ;;
    esac
}

scene_verify() {
    case $1 in
        whole) verify "$2" "$3" ;;
        cuts) echo "$(verify_frames "$3" "$(cuts_total)") $(verify_cuts "$2" "$3")" ;;
    esac
}
