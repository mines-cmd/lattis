import QtQuick
import QtQuick.Effects

// The golden glow behind the icon of a rare achievement. Fills its parent (the icon) and draws past its
// edges, so the parent must not clip; z is -1 so the icon covers the middle.
Item {
    id: glow
    property bool active: false

    z: -1
    visible: active && Theme.achievements.rareGlowEnabled

    Loader {
        anchors.centerIn: parent
        active: glow.visible
        sourceComponent: Rectangle {
            width: glow.width + Theme.achievements.rareGlowSize * glow.width
            height: glow.height + Theme.achievements.rareGlowSize * glow.height
            radius: Theme.achievements.iconRadius + Theme.achievements.rareGlowSize * glow.width / 2
            color: Theme.achievements.rareGlowColor
            opacity: Theme.achievements.rareGlowOpacity
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blurMax: 32
                blur: Theme.achievements.rareGlowSoftness
            }
        }
    }
}
