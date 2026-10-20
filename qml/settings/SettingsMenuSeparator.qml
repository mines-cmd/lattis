import QtQuick
import QtQuick.Controls.Basic

MenuSeparator {
    id: separator

    implicitHeight: visible ? implicitContentHeight + topPadding + bottomPadding : 0
    contentItem: Rectangle {
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
    }
}
