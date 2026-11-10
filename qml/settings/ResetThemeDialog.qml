import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    carded: false
    title: qsTr("Reset %1 to built-in?").arg(name)

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

    Label {
        Layout.fillWidth: true
        text: qsTr("This deletes %1, which holds your changes to the built-in %2. The built-in's own values come back.")
                  .arg((root.half === "colors" ? "themes/colors/" : "themes/styles/") + root.name + ".toml")
                  .arg(root.name)
        wrapMode: Text.WordWrap
        color: Theme.controls.descriptionColor
    }

    footer: [
        SettingsButton { text: qsTr("Cancel"); onClicked: root.close() },
        SettingsButton {
            primary: true
            text: qsTr("Reset")
            onClicked: {
                root.close()
                if (Theme.resetToBuiltIn(root.half))
                    root.done(qsTr("%1 reset to built-in").arg(root.name))
            }
        }
    ]
}
