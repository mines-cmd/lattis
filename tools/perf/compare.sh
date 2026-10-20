#!/bin/sh
# Runs tools/screenshot.sh alternately against two builds (A, B, A, B...), each run on a
# fresh copy of a data dir, and prints a table of the first frame and memory lines.
#
#   tools/perf/compare.sh <lattis A> <lattis B> <runs> <data dir setup> [delay ms]
#
# <data dir setup> is a folder to copy as the data dir for each run (a demo library, or
# LIBRARY_BENCH_KEEP's files/ with art), or "--demo" for tools/demo-data. "--cold" in front
# of it (an extra first argument) leaves the cache out, so each run is a first start;
# otherwise one run warms the cache first and every measured run has it. Every setup must
# point [steam] at a fake Steam or disable it (see the performance review's Guardrails):
# the "Steam at" line is checked for each run and the script stops if it names the real one.
#
# The caches and state go next to the copies, in a scratch folder (LATTIS_PERF_TMP, or a
# new one under $TMPDIR) that is removed at the end.
set -e

root=$(realpath "$(dirname "$0")/../..")
tools=$root/tools
warm=1
if [ "$1" = "--cold" ]; then
    warm=0
    shift
fi
a=$1 b=$2 runs=$3 setup=$4 delay=${5:-2500}
[ -n "$a" ] && [ -n "$b" ] && [ -n "$runs" ] && [ -n "$setup" ] || {
    echo "usage: $0 [--cold] <lattis A> <lattis B> <runs> <data dir setup | --demo> [delay ms]" >&2
    exit 2
}

a=$(realpath "$a")
b=$(realpath "$b")
[ "$setup" = "--demo" ] || setup=$(realpath "$setup")
work=$(realpath "${LATTIS_PERF_TMP:-$(mktemp -d)}")
mkdir -p "$work"
trap 'command rm -rf "$work"' EXIT

fresh() { # fresh <name>: a new copy of the setup as $work/<name>
    command rm -rf "$work/$1" "$work/$1-cache" "$work/$1-state"
    mkdir -p "$work/$1"
    if [ "$setup" = "--demo" ]; then
        cp -r "$tools/demo-data/." "$work/$1/"
    else
        cp -r "$setup/." "$work/$1/"
    fi
}

run() { # run <binary> <name> <label>: prints "<label> <first frame> <rss> <peak>"
    log="$work/$2.log"
    LATTIS_BINARY=$1 LATTIS_LOG="$log" tools/screenshot.sh "$work/$2" "$work/$2.png" "$delay" >/dev/null
    grep -q "^data dir: $work/" "$log" || { echo "stop: $3 did not run on the scratch data dir" >&2; exit 1; }
    if grep -q "Steam at " "$log" && ! grep "Steam at " "$log" | grep -q "Steam at $work/"; then
        echo "stop: $3 found a Steam outside the scratch folder:" >&2
        grep "Steam at " "$log" >&2
        exit 1
    fi
    frame=$(sed -n 's/^first frame after \([0-9]*\) ms/\1/p' "$log")
    mem=$(sed -n 's/^memory: \([0-9.]*\) MB, peak \([0-9.]*\) MB/\1 \2/p' "$log" | tail -1)
    echo "$3 ${frame:-?} ${mem:-? ?}"
}

cd "$root"
printf '%-4s %-4s %12s %10s %10s\n' side run first_frame_ms rss_MB peak_MB
i=1
while [ "$i" -le "$runs" ]; do
    for side in A B; do
        if [ "$side" = A ]; then bin=$a; else bin=$b; fi
        name=$side
        fresh "$name"
        if [ "$warm" = 1 ]; then
            # One start to write the cache, which the measured start then reads.
            LATTIS_BINARY=$bin tools/screenshot.sh "$work/$name" "$work/$name.png" "$delay" >/dev/null
        fi
        result=$(run "$bin" "$name" "$side") || exit 1
        set -- $result
        printf '%-4s %-4s %12s %10s %10s\n' "$side" "$i" "$2" "$3" "$4"
    done
    i=$((i + 1))
done
