import QtQuick
import QtQuick.Effects

Item {
    RectangularShadow {
        visible: Theme.menu.popupShadowSize > 0
        anchors.fill: parent
        offset.y: Theme.menu.popupShadowSize / 6
        radius: Theme.controls.inputRadius
        blur: Theme.menu.popupShadowSize
        color: Theme.menu.popupShadowColor
    }
    Rectangle {
        anchors.fill: parent
        color: Theme.menu.popupBackground
        border.color: Theme.settingsWindow.cardBorder
        border.width: Theme.settingsWindow.cardBorderWidth
        radius: Theme.controls.inputRadius
    }
}
