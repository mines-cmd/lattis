import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A running emulator install in a small space: a short bar and one word ("47 %", "Unpacking").
RowLayout {
    id: root

    // The emulator's Installer.emulators row.
    required property var info
    readonly property string state: info?.state ?? "idle"

    readonly property string label: {
        switch (state) {
        case "checking": return qsTr("Checking")
        case "downloading":
            return info.size > 0 && info.progress >= 0 ? qsTr("%1 %").arg(Math.round(info.progress * 100))
                                                       : qsTr("Downloading")
        case "extracting": return qsTr("Unpacking")
        case "installing": return qsTr("Installing")
        case "removing": return qsTr("Uninstalling")
        }
        return ""
    }

    spacing: 8

    InstallProgress {
        Layout.preferredWidth: Theme.settingsWindow.installMiniProgressWidth
        Layout.alignment: Qt.AlignVCenter
        progress: root.state === "downloading" ? root.info.progress : -1
    }
    Label {
        text: root.label
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.settingsWindow.sourceTagFontSize
    }
}
