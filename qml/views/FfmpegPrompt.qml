import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property color textColor: Theme.details.descriptionColor
    property bool showNotNow: true
    readonly property bool busy: FfmpegTool.state !== "idle" && FfmpegTool.state !== "failed"

    signal notNow()

    spacing: Theme.layout.spacing

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: root.textColor
        text: FfmpegTool.state === "downloading" ? qsTr("Downloading ffmpeg")
            : root.busy ? qsTr("Installing ffmpeg")
            : FfmpegTool.state === "failed" ? FfmpegTool.error
            : qsTr("Clips need ffmpeg to play. Lattis can download it for you (about %1 MB, from github.com/BtbN/FFmpeg-Builds).")
                  .arg(Math.max(1, Math.round((FfmpegTool.size > 0 ? FfmpegTool.size : 70000000) / 1000000)))
    }
    InstallProgress {
        visible: root.busy
        Layout.fillWidth: true
        progress: FfmpegTool.state === "downloading" ? FfmpegTool.progress : -1
    }
    RowLayout {
        spacing: Theme.layout.spacing

        SettingsButton {
            visible: !root.busy
            text: qsTr("Install ffmpeg")
            primary: true
            onClicked: FfmpegTool.install()
        }
        SettingsButton {
            visible: !root.busy && root.showNotNow
            text: qsTr("Not now")
            onClicked: {
                FfmpegTool.cancel()
                root.notNow()
            }
        }
        SettingsButton {
            visible: root.busy
            text: qsTr("Cancel")
            onClicked: FfmpegTool.cancel()
        }
    }
}
