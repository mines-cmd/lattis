#!/bin/sh
# Runs a command with some programs hidden or broken, to test states this machine can't
# produce (no ffmpeg, no flatpak, a failing tool).
#
#   tools/without.sh [--fail] <program>[,<program>…] -- <command…>
#
# Every <program> found on PATH is replaced inside a bubblewrap sandbox (the rest of the
# system is untouched): hidden, so lookups and execs fail, or with --fail, swapped for a
# script that prints an error and exits 1. Works with tools/screenshot.sh:
#
#   tools/without.sh ffmpeg -- tools/screenshot.sh /tmp/d /tmp/o.png
#
# Needs bwrap. Programs in directories the sandbox can't override (none on a usual PATH)
# are reported and skipped.
set -e

mode=hide
if [ "$1" = "--fail" ]; then mode=fail; shift; fi
programs=$1
shift
[ "$1" = "--" ] && shift
[ -n "$programs" ] && [ $# -gt 0 ] || { sed -n '2,12p' "$0"; exit 2; }

stub=$(mktemp)
trap 'command rm -f "$stub"' EXIT
printf '#!/bin/sh\necho "%s: simulated failure" >&2\nexit 1\n' "\${0##*/}" > "$stub"
chmod +x "$stub"

args=
for name in $(echo "$programs" | tr ',' ' '); do
    found=
    for path in $(which -a "$name" 2>/dev/null | sort -u); do
        real=$(realpath "$path")
        found=1
        if [ "$mode" = fail ]; then
            args="$args --ro-bind $stub $path --ro-bind $stub $real"
        else
            args="$args --ro-bind /dev/null $path --ro-bind /dev/null $real"
        fi
    done
    [ -n "$found" ] || echo "without: $name isn't installed here already" >&2
done

# shellcheck disable=SC2086
exec bwrap --dev-bind / / $args "$@"
