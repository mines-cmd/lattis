#!/bin/sh
# Builds Lattis-<version>-x86_64.AppImage in an Ubuntu 22.04 container, with the official Qt.
#
#   tools/appimage.sh [--install] [--version <x.y.z>] [--update-info]
#
# Each run raises the version in CMakeLists.txt's project() first, so each build has its own
# number: 0.9 becomes 0.9.1, 0.9.1 becomes 0.9.2. --version sets it instead. A failed build
# puts the old version back.
#
# --update-info is for releases (tools/publish.sh passes it): the AppImage tells Gear Lever and
# AppImageUpdate to look for newer ones in the public repo's GitHub releases, and a .zsync file
# made beside it lets them download only what changed. Local builds leave it out, or an
# updater would replace them with the latest release.
#
# The build runs in the lattis-appimage podman image (tools/appimage/Containerfile), made the
# first time and whenever the Containerfile is newer than it. Ubuntu 22.04 has glibc 2.35, so
# the AppImage runs on that or anything newer. tools/appimage/build.sh is the part that runs
# inside: it configures, builds and bundles with linuxdeploy, and puts the licence texts of
# what it bundles in usr/share/licenses (the LGPL asks that Qt's source can be found).
# Output: build/appimage/Lattis-<version>-x86_64.AppImage. --install then copies it over
# ~/AppImages/lattis.appimage, where Gear Lever keeps the copy it integrated; its menu entry
# points there, so the next start runs the new build.
set -e

usage() { echo "usage: $0 [--install] [--version <x.y.z>] [--update-info]" >&2; exit 2; }
install=
version=
update_info=
while [ $# -gt 0 ]; do
    case "$1" in
        --install) install=1 ;;
        --update-info) update_info=1 ;;
        --version) [ $# -gt 1 ] || usage; version=$2; shift ;;
        --version=*) version=${1#--version=} ;;
        *) usage ;;
    esac
    shift
done
# CMake takes up to four numeric components.
if [ -n "$version" ] && ! echo "$version" | grep -Eqx '[0-9]+(\.[0-9]+){0,3}'; then
    echo "$0: version must be numbers separated by dots, like 0.9.1" >&2
    exit 2
fi

repo=$(realpath "$(dirname "$0")/..")
cmakelists="$repo/CMakeLists.txt"
image=localhost/lattis-appimage
containerfile="$repo/tools/appimage/Containerfile"

# Before the version changes, so a missing podman or a failed image build leaves nothing to undo.
command -v podman >/dev/null || { echo "$0: podman is needed to build the AppImage" >&2; exit 1; }
rebuild=
if ! podman image exists "$image"; then
    rebuild=1
else
    created=$(podman image inspect --format '{{.Created.Unix}}' "$image")
    [ "$(stat -c %Y "$containerfile")" -gt "$created" ] && rebuild=1
fi
if [ -n "$rebuild" ]; then
    echo "Building the $image image (the first time takes a while)"
    podman build -t "$image" "$repo/tools/appimage"
fi

old_version=$(sed -n 's/^project(Lattis VERSION \([^ ]*\).*/\1/p' "$cmakelists")
if [ -z "$version" ]; then
    # 0.9 -> 0.9.1, 0.9.1 -> 0.9.2: the third number goes up, starting at 1.
    version=$(echo "$old_version" | awk -F. '{ printf "%s.%s.%d\n", $1, ($2 == "" ? 0 : $2), $3 + 1 }')
fi
sed -i "s/^project(Lattis VERSION $old_version /project(Lattis VERSION $version /" "$cmakelists"
echo "Version $old_version -> $version"

cleanup() {
    status=$?
    if [ "$status" -ne 0 ]; then
        sed -i "s/^project(Lattis VERSION $version /project(Lattis VERSION $old_version /" "$cmakelists"
        echo "Build failed; version back to $old_version" >&2
    fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM
out="$repo/build/appimage"
mkdir -p "$out"

# An empty LDAI_UPDATE_INFORMATION could still count as set, so it is only passed for releases.
set -- -e LATTIS_VERSION="$version"
if [ -n "$update_info" ]; then
    set -- "$@" -e "LDAI_UPDATE_INFORMATION=gh-releases-zsync|mines-cmd|lattis|latest|Lattis-*x86_64.AppImage.zsync"
fi
# keep-id: the files the build writes into the mounted repo stay the user's. label=disable:
# the repo is not relabelled for SELinux.
podman run --rm --userns=keep-id --security-opt label=disable -v "$repo:/src" "$@" \
    "$image" /src/tools/appimage/build.sh

# Replaced through a temporary file and a rename, so a running Lattis keeps its old file.
if [ -n "$install" ]; then
    target="$HOME/AppImages/lattis.appimage"
    mkdir -p "$(dirname "$target")"
    cp "$out/Lattis-$version-x86_64.AppImage" "$target.new"
    chmod +x "$target.new"
    mv "$target.new" "$target"
    echo "Installed $target"
fi
