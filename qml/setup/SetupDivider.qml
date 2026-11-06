import QtQuick
import QtQuick.Layouts

Rectangle {
    visible: Theme.settingsWindow.rowDividers
    Layout.fillWidth: true
    Layout.leftMargin: -Theme.settingsWindow.rowPadding
    Layout.rightMargin: -Theme.settingsWindow.rowPadding
    implicitHeight: 1
    color: Theme.settingsWindow.rowDivider
}
