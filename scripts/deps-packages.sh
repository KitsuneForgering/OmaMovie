# Source the PKGBUILD in a subshell so its variables and functions do not leak here.
read_array() {
    (
        # shellcheck source=/dev/null
        source "$pkgbuild"
        declare -n arr="$1"
        printf '%s\n' "${arr[@]}"
    )
}

packages=()
mapfile -t -O "${#packages[@]}" packages < <(read_array depends)
if ((!runtime_only)); then
    mapfile -t -O "${#packages[@]}" packages < <(read_array makedepends)
    mapfile -t -O "${#packages[@]}" packages < <(read_array checkdepends)
    if ((dev)); then
        mapfile -t -O "${#packages[@]}" packages < <(read_array _devdepends)
    fi
fi
# Drop version constraints (pkg>=1.0) and empty entries, keep order, remove duplicates.
mapfile -t packages < <(printf '%s\n' "${packages[@]}" | sed -E 's/[<>=].*$//' | awk 'NF && !seen[$0]++')
