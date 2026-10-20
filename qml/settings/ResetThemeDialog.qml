import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    property string half: "colors"
    property string name

    signal done(string text)

    function openFor(half, name) {
        if (!Theme.canResetToBuiltIn(half))
            return
        root.half = half
        root.name = name
        open()
    }

    anchors.centerIn: parent
    modal: true
    Overlay.modal: Scrim {}
    padding: Theme.settingsWindow.dialogPadding
    width: 340

    background: Rectangle {
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth
        radius: Theme.settingsWindow.inputRadius
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            text: qsTr("Reset %1 to built-in?").arg(root.name)
            color: Theme.settingsWindow.labelColor
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("This deletes %1, which holds your changes to the built-in %2. The built-in's own values come back.")
                      .arg((root.half === "colors" ? "themes/colors/" : "themes/styles/") + root.name + ".toml")
                      .arg(root.name)
            wrapMode: Text.WordWrap
            color: Theme.settingsWindow.descriptionColor
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.settingsWindow.controlSpacing
            SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
            SettingsButton {
                primary: true
                text: qsTr("Reset")
                onClicked: {
                    root.close()
                    if (Theme.resetToBuiltIn(root.half))
                        root.done(qsTr("%1 reset to built-in").arg(root.name))
                }
            }
        }
    }
}
