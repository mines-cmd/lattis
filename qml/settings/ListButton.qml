import QtQuick

SettingsButton {
    implicitHeight: Theme.settingsWindow.inputHeight - 8
    leftPadding: (implicitHeight - Theme.settingsWindow.iconSize) / 2
    rightPadding: leftPadding
}
