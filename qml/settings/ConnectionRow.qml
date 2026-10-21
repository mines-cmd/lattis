import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root
    property bool ready
    property string needsKeysText: qsTr("Add the keys above to test them")
    property bool busy
    // "" = works, undefined = not tested
    property var result
    required property real buttonWidth
    signal test()
    Layout.fillWidth: true
    spacing: Theme.layout.spacing

    Label {
        Layout.preferredWidth: Theme.settingsWindow.labelMinWidth
        text: qsTr("Connection")
        color: Theme.controls.labelColor
        font.pixelSize: Theme.controls.labelFontSize
    }
    Icon {
        opacity: root.result === "" && !root.busy ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
        path: Icons.check
        size: Theme.controls.iconSize
        color: status.color
    }
    Label {
        id: status
        Layout.fillWidth: true
        text: root.busy ? qsTr("Testing…")
            : root.result === "" ? qsTr("Works")
            : root.result !== undefined ? root.result
            : root.ready ? qsTr("Not tested yet") : root.needsKeysText
        color: root.result === "" && !root.busy ? Theme.settingsWindow.installFound
             : root.result !== undefined && !root.busy ? Theme.controls.problemsText
             : Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.labelFontSize
        elide: Text.ElideRight
        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }
    SettingsButton {
        text: qsTr("Test")
        Layout.preferredWidth: root.buttonWidth
        enabled: root.ready && !root.busy
        onClicked: root.test()
    }
}
