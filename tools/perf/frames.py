#!/usr/bin/env python3
"""Summarizes the frame times in a log made with QSG_RENDER_TIMING=1.

    python3 -I tools/perf/frames.py <log> [<log>...]

A frame's time is the render thread's "frame rendered in N ms" (sync, render and swap).
Frames that rendered nothing (0 ms) are left out of the statistics and counted apart.
Prints the number of frames, the median and 95th percentile time, the longest, and the
frames over 25 ms (a dropped frame at 60 Hz; the budget is 16.7 ms).
"""
import re
import statistics
import sys

RENDERED = re.compile(r"frame rendered in (\d+)ms")
DROPPED_MS = 25


def percentile(sorted_times, fraction):
    return sorted_times[min(len(sorted_times) - 1, int(len(sorted_times) * fraction))]


def summarize(path):
    times = []
    empty = 0
    with open(path, errors="replace") as log:
        for line in log:
            match = RENDERED.search(line)
            if not match:
                continue
            ms = int(match.group(1))
            if ms == 0:
                empty += 1
            else:
                times.append(ms)
    if not times:
        print(f"{path}: no frames (was QSG_RENDER_TIMING=1 set? {empty} empty frames)")
        return
    times.sort()
    over = sum(1 for t in times if t > DROPPED_MS)
    print(f"{path}: {len(times)} frames (+{empty} empty), median {statistics.median(times):g} ms, "
          f"p95 {percentile(times, 0.95)} ms, max {times[-1]} ms, over {DROPPED_MS} ms: {over}")


if len(sys.argv) < 2:
    sys.exit(__doc__)
for log in sys.argv[1:]:
    summarize(log)
