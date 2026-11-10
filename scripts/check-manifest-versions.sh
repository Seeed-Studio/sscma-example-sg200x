#!/usr/bin/env bash
#
# Report drift between an application's package version and every place its
# gallery manifest claims a version.
#
# The single source of truth is `project(<id> VERSION x.y.z)` in the solution's
# CMakeLists.txt: CPack names the deb from it and opkg records it. Everything
# else is a copy.
#
# There are two kinds of copy, and only one of them can be generated away:
#
#   1. solutions/<id>/rootfs/usr/share/<id>/<id>.json
#      Ships inside the app's own deb. cmake/package.cmake overwrites its
#      version with PROJECT_VERSION at package time, so what installs is always
#      correct -- but a stale value in git still misleads whoever reads it, so
#      it is checked here too.
#
#   2. solutions/supervisor/rootfs/usr/share/supervisor/apps/<id>.json
#      Ships inside the *supervisor* deb so the gallery can advertise apps that
#      are not installed yet. The supervisor build cannot see another
#      solution's PROJECT_VERSION, so nothing generates this one. It is the
#      copy that rots quietly, and the only defence is this check.
#      Where the app also ships its own manifest the two must be identical --
#      the built-in is meant to be a copy, not a variant.
#
# Usage:
#   scripts/check-manifest-versions.sh          # report drift, exit 1 if any
#   scripts/check-manifest-versions.sh --fix    # rewrite the copies, then report
#
# zsh-compatible: no associative arrays, no bash-4 features (macOS ships 3.2).

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOLUTIONS="$REPO_ROOT/solutions"
BUILTIN_DIR="$SOLUTIONS/supervisor/rootfs/usr/share/supervisor/apps"

FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

drift=0
fixed=0

json_version() {
    # First "version": "..." at the top level of a pretty-printed manifest.
    grep -oE '^  "version"[[:blank:]]*:[[:blank:]]*"[^"]*"' "$1" 2>/dev/null |
        head -1 | sed 's/.*"\([^"]*\)"$/\1/'
}

set_json_version() {
    # In-place rewrite of that same field. Keeps the rest of the file byte-identical.
    local file="$1" ver="$2" tmp
    tmp="$(mktemp)"
    sed "s|^  \"version\"[[:blank:]]*:[[:blank:]]*\"[^\"]*\"|  \"version\": \"$ver\"|" "$file" > "$tmp"
    mv "$tmp" "$file"
}

printf '%-22s %-10s %-12s %s\n' "APP" "PACKAGE" "OWN MANIFEST" "SUPERVISOR BUILT-IN"
printf '%s\n' "----------------------------------------------------------------------------"

for dir in "$SOLUTIONS"/*/; do
    id="$(basename "$dir")"
    [ "$id" = "supervisor" ] && continue

    cmakelists="$dir/CMakeLists.txt"
    [ -f "$cmakelists" ] || continue

    pkg_ver="$(sed 's/#.*//' "$cmakelists" | tr '\n\r\t' '   ' |
        grep -oE "project\([[:blank:]]*$id[[:blank:]]+VERSION[[:blank:]]+[0-9][0-9.]*" |
        grep -oE '[0-9][0-9.]*$')"
    [ -n "$pkg_ver" ] || continue   # no VERSION declared -> nothing to compare against

    own="$dir/rootfs/usr/share/$id/$id.json"
    builtin="$BUILTIN_DIR/$id.json"

    own_ver="-"; own_bad=0
    if [ -f "$own" ]; then
        own_ver="$(json_version "$own")"
        [ "$own_ver" = "$pkg_ver" ] || own_bad=1
    fi

    builtin_ver="-"; builtin_bad=0
    if [ -f "$builtin" ]; then
        builtin_ver="$(json_version "$builtin")"
        [ "$builtin_ver" = "$pkg_ver" ] || builtin_bad=1
    fi

    if [ "$FIX" -eq 1 ]; then
        [ "$own_bad" -eq 1 ] && { set_json_version "$own" "$pkg_ver"; own_ver="$pkg_ver"; own_bad=0; fixed=$((fixed + 1)); }
        [ "$builtin_bad" -eq 1 ] && { set_json_version "$builtin" "$pkg_ver"; builtin_ver="$pkg_ver"; builtin_bad=0; fixed=$((fixed + 1)); }
    fi

    mark_own=""; [ "$own_bad" -eq 1 ] && { mark_own=" <-- drift"; drift=$((drift + 1)); }
    mark_bi="";  [ "$builtin_bad" -eq 1 ] && { mark_bi=" <-- drift"; drift=$((drift + 1)); }

    printf '%-22s %-10s %-12s %s%s%s\n' \
        "$id" "$pkg_ver" "${own_ver}${mark_own}" "$builtin_ver" "$mark_bi" ""

    # A built-in that exists alongside the app's own manifest is supposed to be
    # a verbatim copy. Diverging content (not just the version) means the
    # gallery advertises one thing and installs another.
    if [ -f "$own" ] && [ -f "$builtin" ] && ! diff -q "$own" "$builtin" >/dev/null 2>&1; then
        echo "    ! built-in copy differs from the app's own manifest beyond the version field"
        echo "      $builtin"
        echo "      $own"
        drift=$((drift + 1))
    fi
done

echo
if [ "$FIX" -eq 1 ]; then
    echo "rewrote $fixed field(s)"
fi
if [ "$drift" -gt 0 ]; then
    echo "FAIL: $drift version(s) out of sync with project(<id> VERSION ...)."
    echo "      Run '$0 --fix', or bump project(VERSION) if the manifest is the correct one."
    exit 1
fi
echo "OK: every manifest version matches its package version."
