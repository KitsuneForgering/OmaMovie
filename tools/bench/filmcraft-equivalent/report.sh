# One folder per day under the report directory, kept for kRetentionDays. Sourced by run.sh.

kRetentionDays=7

# Deletes day folders older than the retention window; only names shaped like a date are touched.
prune_old_runs() {
    local out_dir=$1 oldest dir
    oldest=$(date -d "$((kRetentionDays - 1)) days ago" +%F)
    for dir in "$out_dir"/????-??-??; do
        [[ -d $dir && $(basename "$dir") < $oldest ]] && rm -rf -- "$dir"
    done
}

# The previous run's key lines next to today's, for a quick look at what moved.
compare_with_previous() {
    local out_dir=$1 report=$2 today previous keys
    today=$(basename "$(dirname "$report")")
    previous=$(find "$out_dir" -mindepth 2 -maxdepth 2 -path '*/????-??-??/report.txt' ! -path "*/$today/*" | sort | tail -1)
    [[ -n $previous ]] || return 0
    keys='^(M4|M6|export audit|  (seek|select|split|ripple|undo|drag)|dec_|  composite|  libplacebo|frames over)'
    { echo; echo "== changes since $(basename "$(dirname "$previous")")"
      diff <(grep -E "$keys" "$previous") <(grep -E "$keys" "$report") || true; } >> "$report"
}
