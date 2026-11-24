import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
UpdateRowsDialog {
    id: root

    title: ready ? qsTr("Lattis %1 is ready").arg(AppUpdate.latestVersion)
                 : qsTr("Update to Lattis %1").arg(AppUpdate.latestVersion)

    readonly property string state: AppUpdate.state
    readonly property bool downloading: state === "downloading"
    readonly property bool ready: state === "ready"
    readonly property bool failed: state === "failed"

    rows: [{ id: "lattis", name: "Lattis", from: Qt.application.version, version: AppUpdate.latestVersion,
             size: AppUpdate.size, notes: AppUpdate.notes }]
    showNotes: !downloading && !ready
    subtitle: ready ? qsTr("It replaced the file you started Lattis from. Restart to use it.")
            : downloading ? qsTr("Downloading %1 of %2").arg(Math.max(1, Math.round(AppUpdate.size * Math.max(0, AppUpdate.progress) / 1e6)))
                                   .arg(megabytes(AppUpdate.size))
            : ""
    stateOf: () => ({ status: downloading ? "active" : failed ? "failed" : ready ? "done" : "pending",
                      progress: AppUpdate.progress, step: "", error: AppUpdate.error })

    signal message(string text)

    closePolicy: Popup.CloseOnEscape

    function skipVersion() {
        AppUpdate.skip()
        close()
    }

    function restartNow() {
        AppUpdate.restart()
        if (AppUpdate.error !== "")
            message(AppUpdate.error)
    }

    Label {
        visible: !AppUpdate.canReplace && !root.ready
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: AppUpdate.replaceBlocker
    }

    Label {
        visible: AppUpdate.canReplace && AppUpdate.noChecksum && !root.ready && !root.downloading
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.warningColor
        text: qsTr("This release has no checksum. Lattis can't check the download, so it won't update itself this time. "
                   + "Download it from the release page instead.")
    }

    Label {
        visible: root.ready && AppUpdate.notice !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.warningColor
        text: AppUpdate.notice
    }

    Label {
        visible: root.ready && AppUpdate.runningGame !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("Close %1 first.").arg(AppUpdate.runningGame)
    }

    footer: [
        SettingsButton {
            visible: !root.downloading && !root.ready
            text: qsTr("Skip this version")
            onClicked: root.skipVersion()
        },
        SettingsButton {
            visible: !root.downloading
            text: qsTr("Later")
            onClicked: root.close()
        },
        SettingsButton {
            visible: root.downloading
            text: qsTr("Cancel")
            onClicked: AppUpdate.cancel()
        },
        SettingsButton {
            visible: !root.downloading && !root.ready && (!AppUpdate.canReplace || AppUpdate.noChecksum)
            text: qsTr("Open release page")
            primary: true
            onClicked: GameRunner.openUrl(AppUpdate.releasePage)
        },
        SettingsButton {
            visible: !root.downloading && !root.ready && AppUpdate.canReplace && !AppUpdate.noChecksum
            text: root.failed ? qsTr("Try again") : qsTr("Update")
            primary: true
            onClicked: AppUpdate.update()
        },
        SettingsButton {
            visible: root.ready
            text: qsTr("Restart now")
            primary: true
            enabled: AppUpdate.runningGame === ""
            onClicked: root.restartNow()
        }
    ]
}
