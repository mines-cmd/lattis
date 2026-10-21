import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root

    property string text
    property bool conflict: false
    property bool recording: false
    property bool muted: false

    readonly property color outline: recording ? Theme.settingsWindow.keyChipRecording
                                   : conflict ? Theme.settingsWindow.keyChipConflict
                                   : Theme.settingsWindow.keyChipBorder

    implicitWidth: label.implicitWidth + Theme.settingsWindow.keyChipPadding * 2
    implicitHeight: Theme.settingsWindow.keyChipHeight
    radius: Theme.settingsWindow.keyChipRadius
    color: muted ? Qt.alpha(Theme.settingsWindow.keyChipBackground, 0) : Theme.settingsWindow.keyChipBackground
    border.color: outline
    border.width: recording || conflict ? Math.max(1, Theme.settingsWindow.keyChipBorderWidth)
                                        : Theme.settingsWindow.keyChipBorderWidth

    Behavior on color { enabled: Motion.fades; MotionColor {} }
    Behavior on border.color { enabled: Motion.fades; MotionColor {} }

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.conflict ? Theme.settingsWindow.keyChipConflict
             : root.muted ? Theme.controls.descriptionColor
             : Theme.settingsWindow.keyChipText
        font.pixelSize: Theme.settingsWindow.keyChipFontSize

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }
}
