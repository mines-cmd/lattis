import QtQuick

// See docs/code/qml.md.
Surface {
    id: root

    property real hue: 0

    direction: "right"
    fill: Qt.hsla(hue, Theme.hero.placeholderSaturation, Theme.hero.placeholderLightness, 1)
    fillEnd: Qt.hsla((hue + Theme.hero.placeholderHueShift) % 1, Theme.hero.placeholderEndSaturation,
                     Theme.hero.placeholderEndLightness, 1)
}
