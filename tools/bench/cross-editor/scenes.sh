# Scenes: what each editor exports and how the output is checked. Sourced by run.sh after
# editors.sh, verify.sh and cuts.sh; sets oma_in (one path) and melt_in (melt producer args).
#   whole  the source exported as it is
#   cuts   five reordered cuts (cuts.sh), checked frame by frame against the source

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
        *) echo "unknown scene: $scene" >&2; return 1 ;;
    esac
}

scene_verify() {
    case $1 in
        whole) verify "$2" "$3" ;;
        cuts) echo "$(verify_frames "$3" "$(cuts_total)") $(verify_cuts "$2" "$3")" ;;
    esac
}
