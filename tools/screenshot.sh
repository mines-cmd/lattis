#!/bin/sh
# Screenshots the app without touching the real screen.
#
#   tools/screenshot.sh [--demo] <data dir> <out.png> [delay ms]
#
# --demo copies the demo library (tools/demo-data) into the data dir first. The library
# cache and the remembered window sizes go next to the data dir (<data dir>-cache,
# <data dir>-state), not into ~/.cache and ~/.local/state.
#
# Runs the app inside a headless KWin session, so it renders on the GPU (unlike
# QT_QPA_PLATFORM=offscreen, which falls back to software rendering and skips effects such
# as the rounded hero corners and the blurred backdrop). Needs kwin_wayland.
#
# For measuring: LATTIS_BINARY=<path> runs that
# binary instead of build/lattis; LATTIS_WRAPPER="<command>" goes in front of it
# (heaptrack -o <file>); LATTIS_LOG=<file> keeps the app's whole output, not only the
# lines printed here; LATTIS_TIMEOUT=<seconds> (default 40) is how long the session may run.
# QSG_RENDER_TIMING is passed on, and LATTIS_SCROLL is passed on with the other hooks.
set -e

tools=$(dirname "$0")
if [ "$1" = "--demo" ]; then
    shift
    mkdir -p "$1"
    cp -r "$tools/demo-data/." "$1/"
fi

data=$(realpath "$1")
out=$(realpath "$2")
delay=${3:-2500}
app=$(realpath "${LATTIS_BINARY:-$tools/../build/lattis}")

keep_log() {
    if [ -n "$LATTIS_LOG" ]; then tee "$LATTIS_LOG"; else cat; fi
}

unset DISPLAY
timeout "${LATTIS_TIMEOUT:-40}" kwin_wayland --virtual --no-lockscreen --socket lattis-headless \
    --width 1600 --height 950 --exit-with-session \
    "env QT_QPA_PLATFORM=wayland QT_FORCE_STDERR_LOGGING=1 ${QSG_RENDER_TIMING:+QSG_RENDER_TIMING=$QSG_RENDER_TIMING} LATTIS_DATA=$data XDG_CACHE_HOME=$data-cache XDG_STATE_HOME=$data-state \
     LATTIS_SCREENSHOT=$out LATTIS_SCREENSHOT_DELAY=$delay LATTIS_OPEN_SETTINGS=${LATTIS_OPEN_SETTINGS:-0} LATTIS_SCREENSHOT_SETTINGS=${LATTIS_SCREENSHOT_SETTINGS:-} LATTIS_SCREENSHOT_LAUNCH=${LATTIS_SCREENSHOT_LAUNCH:-} LATTIS_SELECT_GAME=${LATTIS_SELECT_GAME:-} LATTIS_SCROLL=${LATTIS_SCROLL:-} LATTIS_OPEN_FILTERS=${LATTIS_OPEN_FILTERS:-0} LATTIS_RAIL_MENU=${LATTIS_RAIL_MENU:-0} LATTIS_EDIT_GAME=${LATTIS_EDIT_GAME:-} LATTIS_UNINSTALL=${LATTIS_UNINSTALL:-0} LATTIS_STEAM_UNDO=${LATTIS_STEAM_UNDO:-0} LATTIS_GAME_MENU=${LATTIS_GAME_MENU:-} LATTIS_LOGO_MENU=${LATTIS_LOGO_MENU:-} LATTIS_SETUP_STEP=${LATTIS_SETUP_STEP:-} LATTIS_ACTION=${LATTIS_ACTION:-} LATTIS_STATS=${LATTIS_STATS:-} ${LATTIS_WRAPPER:-} $app" 2>&1 \
    | keep_log \
    | grep -E "^(launcher\.|data dir|first frame|memory:|qml|QQml)" || true
