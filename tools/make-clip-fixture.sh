#!/bin/sh
# Makes a tiny Steam clip like the ones in userdata/<user>/gamerecordings/clips/: DASH chunks of a test
# pattern with a tone, a session.mpd and a thumbnail. Used for tests/data/steam; needs ffmpeg.
# Usage: make-clip-fixture.sh <clip folder> <app id> <yyyyMMdd_hhmmss> <seconds>
set -eu
clip=$1
app=$2
stamp=$3
seconds=$4
video="$clip/video/bg_${app}_${stamp}"
mkdir -p "$video"
ffmpeg -y -loglevel error \
    -f lavfi -i "testsrc=size=64x36:rate=10:duration=$seconds" \
    -f lavfi -i "sine=frequency=440:duration=$seconds" \
    -c:v libx264 -preset ultrafast -g 10 -pix_fmt yuv420p -c:a aac -b:a 16k \
    -f dash -seg_duration 1 -use_template 1 -use_timeline 0 \
    -init_seg_name 'init-stream$RepresentationID$.m4s' \
    -media_seg_name 'chunk-stream$RepresentationID$-$Number%05d$.m4s' \
    "$video/session.mpd"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=64x36:rate=1:duration=1" -frames:v 1 "$clip/thumbnail.jpg"
