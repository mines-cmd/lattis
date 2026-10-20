#!/bin/bash
# The container half of tools/appimage.sh: configures, builds and bundles Lattis inside the
# lattis-appimage image, with the repo mounted at /src. Not meant to be run by hand.
#
# Environment from the host script: LATTIS_VERSION, LDAI_UPDATE_INFORMATION (releases only).
set -e

repo=/src
out="$repo/build/appimage"
build="$out/container-build"
appdir="$out/AppDir"
glibc_floor=2.35

# The AppDir goes once the AppImage is made, even after a failure: Qt's QML import scan
# searches the whole source tree, and the Qt modules copied into it make CMake warn.
rm -rf "$appdir"
trap 'rm -rf "$appdir"' EXIT

# The static runtime keeps the binary independent of the target's libstdc++ and libgcc.
cmake -S "$repo" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
cmake --build "$build" --target lattis
DESTDIR="$appdir" cmake --install "$build"

cd "$out"
# QML_SOURCES_PATHS: the QML is scanned for imports (QtQuick, Controls, Effects) to bundle.
# The Wayland plugin isn't bundled by default; without it the app would run through XWayland.
export QML_SOURCES_PATHS="$repo/qml"
export EXTRA_PLATFORM_PLUGINS=libqwayland.so
export LINUXDEPLOY_OUTPUT_VERSION="$LATTIS_VERSION"
export NO_STRIP=1
linuxdeploy="/opt/tools/linuxdeploy-x86_64.AppImage"
"$linuxdeploy" --appdir "$appdir" --plugin qt --icon-file "$repo/resources/logo/icons/lattis-512.png"

# The Wayland client libraries come from the system: they must match its compositor, and the
# bundled libwayland-cursor crashes on load. The Qt plugin copies them regardless of
# --exclude-library, so they go before the second pass, which makes the AppImage.
rm -f "$appdir"/usr/lib/libwayland-*
# GLib stays with the system too: the system GTK and its gio modules (dconf, which holds the
# user's theme) need the glib they were built against; Ubuntu's older copy leaves the file
# dialog unthemed (light).
rm -f "$appdir"/usr/lib/libglib-2* "$appdir"/usr/lib/libgio-2* "$appdir"/usr/lib/libgobject-2* \
    "$appdir"/usr/lib/libgmodule-2* "$appdir"/usr/lib/libgthread-2*
# The Qt plugin leaves out the plugins Qt Quick draws on Wayland's GPU surfaces with (wayland-egl);
# without them the window never renders. The second pass bundles what they link to.
qt_libs=$("$QMAKE" -query QT_INSTALL_LIBS)
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
qt_qml=$("$QMAKE" -query QT_INSTALL_QML)
qt_version=$("$QMAKE" -query QT_VERSION)
cp -r "$qt_plugins/wayland-graphics-integration-client" "$appdir/usr/plugins/"
# The GTK platform theme (QT_QPA_PLATFORMTHEME=gtk3) gives the native GTK file chooser, styled
# by the user's GTK theme; without it Qt falls back to its own unthemed dialog. The GTK stack
# it links to comes from the system, with the pixbuf loaders, schemas and modules that match it.
cp "$qt_plugins/platformthemes/libqgtk3.so" "$appdir/usr/plugins/platformthemes/"
deploy() {
    "$linuxdeploy" --appdir "$appdir" "$@" --exclude-library 'libwayland-*' \
        --exclude-library 'libgtk-3*' --exclude-library 'libgdk-3*' --exclude-library 'libgdk_pixbuf*' \
        --exclude-library 'libglycin*' --exclude-library 'libpango*' --exclude-library 'libcairo*' \
        --exclude-library 'libatk*' --exclude-library 'libatspi*' --exclude-library 'libepoxy*' \
        --exclude-library 'libcloudproviders*' --exclude-library 'libtinysparql*' --exclude-library 'libharfbuzz*' \
        --exclude-library 'libglib-2*' --exclude-library 'libgio-2*' --exclude-library 'libgobject-2*' \
        --exclude-library 'libgmodule-2*' --exclude-library 'libgthread-2*'
}
deploy

# What Lattis never loads goes, by an explicit list: a new Qt can't add things silently.
# PNG is built into QtGui; the other image formats Lattis reads are the plugins kept here.
# Controls keeps Fusion because Qt's own non-native dialogs import QtQuick.Controls without a
# style, and Linux defaults to Fusion.
for plugin in "$appdir"/usr/plugins/imageformats/*.so; do
    case "$(basename "$plugin")" in
        libqjpeg.so|libqwebp.so|libqico.so|libqsvg.so|libqgif.so) ;;
        *) rm "$plugin" ;;
    esac
done
rm -rf "$appdir/usr/translations"
quick="$appdir/usr/qml/QtQuick"
for dir in "$quick"/*/; do
    case "$(basename "$dir")" in
        Controls|Dialogs|Effects|Layouts|Shapes|Templates|Window) ;;
        *) rm -rf "$dir" ;;
    esac
done
for dir in "$quick"/Controls/*/; do
    case "$(basename "$dir")" in
        impl|Basic|Fusion) ;;
        *) rm -rf "$dir" ;;
    esac
done
rm -rf "$appdir/usr/qml/QtQml/WorkerScript" "$appdir/usr/qml/QtQml/XmlListModel"
# Qt's own file and folder dialogs, used when no native one answers, import folderlistmodel.
for dir in "$appdir"/usr/qml/Qt/labs/*/; do
    [ "$(basename "$dir")" = folderlistmodel ] || rm -rf "$dir"
done
# The libraries only those files linked to: linuxdeploy bundled them for the first pass, and
# the second pass only adds, never removes.
for lib in QuickParticles QuickVectorImage QuickVectorImageGenerator QuickVectorImageHelpers \
    QmlLocalStorage Sql QmlXmlListModel \
    QuickControls2Material QuickControls2MaterialStyleImpl QuickControls2Universal \
    QuickControls2UniversalStyleImpl QuickControls2Imagine QuickControls2ImagineStyleImpl \
    QuickControls2FluentWinUI3StyleImpl QuickVirtualKeyboard; do
    rm -f "$appdir"/usr/lib/libQt6$lib.so.*
done

# A kept file that links a Qt library removed above would fail to load on the user's machine.
elfs=$(find "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/plugins" "$appdir/usr/qml" -type f \( -name 'lattis' -o -name '*.so*' \))
for needed in $(readelf -d $elfs 2>/dev/null | grep -o 'libQt6[A-Za-z0-9]*\.so\.6' | sort -u); do
    [ -e "$appdir/usr/lib/$needed" ] || { echo "$needed is linked by a kept file but was trimmed" >&2; exit 1; }
done
# The same for QML modules a kept file imports (style-specific +Material and the like are only
# loaded when that style is chosen, which Lattis never does).
for module in $(find "$appdir/usr/qml" -name '*.qml' -not -path '*/+*' -exec grep -hoE '^import [A-Za-z.]+' {} + | awk '{print $2}' | sort -u); do
    [ -d "$appdir/usr/qml/$(echo "$module" | tr . /)" ] || { echo "$module is imported by a kept QML file but was trimmed" >&2; exit 1; }
done

# The licences are gathered once everything is bundled, so the last pass only makes the AppImage.
# Qt's files (and the ICU and other libraries its install ships beside them) come from the
# official binaries; the rest are Ubuntu packages, found by mapping each bundled file back to
# where it came from.
licenses="$appdir/usr/share/licenses"
{
    echo "Lattis $LATTIS_VERSION bundles Qt $qt_version from the official binaries, and these Ubuntu 22.04 packages."
    echo
    echo "Qt: its source for this exact version is"
    echo "https://download.qt.io/archive/qt/${qt_version%.*}/$qt_version/single/qt-everywhere-src-$qt_version.tar.xz"
    echo "Its licence texts are in qt/. The ICU libraries beside Qt's come with the same install."
    echo
    echo "Each package below is listed with its source package; get it with apt-get source <name>"
    echo "or from https://packages.ubuntu.com/source/jammy/<name>."
    echo
} > "$licenses/SOURCES.txt"
mkdir -p "$licenses/qt"
cp /opt/licenses/qt/* "$licenses/qt/"

packages=
for file in $(find "$appdir/usr/lib" "$appdir/usr/plugins" "$appdir/usr/qml" -type f -name '*.so*' 2>/dev/null); do
    name=$(basename "$file")
    case "$file" in
        "$appdir"/usr/plugins/*) [ -e "$qt_plugins/${file#$appdir/usr/plugins/}" ] && continue ;;
        "$appdir"/usr/qml/*) [ -e "$qt_qml/${file#$appdir/usr/qml/}" ] && continue ;;
        *) [ -e "$qt_libs/$name" ] && continue ;;
    esac
    original=$(find /usr/lib /lib -name "$name" -print -quit 2>/dev/null)
    [ -n "$original" ] || { echo "No source found for bundled $name" >&2; exit 1; }
    original=$(realpath "$original")
    package=$(dpkg -S "$original" 2>/dev/null | head -n 1 | cut -d: -f1) || true
    [ -n "$package" ] || package=$(dpkg -S "${original#/usr}" 2>/dev/null | head -n 1 | cut -d: -f1) || true
    [ -n "$package" ] || { echo "No package owns $original ($name)" >&2; exit 1; }
    packages="$packages $package"
done
for package in $(echo $packages | tr ' ' '\n' | sort -u); do
    dpkg-query -W -f='${binary:Package}: ${source:Package} ${source:Version}\n' "$package" >> "$licenses/SOURCES.txt"
    mkdir -p "$licenses/$package"
    cp "/usr/share/doc/$package/copyright" "$licenses/$package/"
done

# The floor the brief promises: nothing bundled may need a newer glibc, and the static runtime
# must leave the executable without a libstdc++ dependency.
newest_glibc=$(objdump -T "$appdir/usr/bin/lattis" $(find "$appdir/usr/lib" "$appdir/usr/plugins" "$appdir/usr/qml" -type f -name '*.so*') 2>/dev/null \
    | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -Vu | tail -n 1)
echo "Newest glibc symbol in the bundle: $newest_glibc (floor $glibc_floor)"
if [ "$(printf '%s\n%s\n' "$newest_glibc" "$glibc_floor" | sort -V | tail -n 1)" != "$glibc_floor" ]; then
    echo "The bundle needs glibc $newest_glibc, newer than $glibc_floor" >&2
    exit 1
fi
if readelf -d "$appdir/usr/bin/lattis" | grep -q 'libstdc++'; then
    echo "bin/lattis still links libstdc++ dynamically" >&2
    exit 1
fi

deploy --output appimage
echo "Bundle: $(du -sm "$appdir" | cut -f1) MB unpacked, $(ls "$appdir/usr/lib" | wc -l) libraries in usr/lib"
