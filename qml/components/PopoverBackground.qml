import QtQuick
import QtQuick.Effects

Item {
    RectangularShadow {
        visible: Theme.settingsWindow.popupShadowSize > 0
        anchors.fill: parent
        offset.y: Theme.settingsWindow.popupShadowSize / 6
        radius: Theme.settingsWindow.inputRadius
        blur: Theme.settingsWindow.popupShadowSize
        color: Theme.settingsWindow.popupShadowColor
    }
    Rectangle {
        anchors.fill: parent
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.settingsWindow.cardBorder
        border.width: Theme.settingsWindow.cardBorderWidth
        radius: Theme.settingsWindow.inputRadius
    }
}
