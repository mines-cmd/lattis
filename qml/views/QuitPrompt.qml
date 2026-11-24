import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var titles: []
    // The Epic games Legendary is updating or has queued: quitting stops them, and Update resumes them.
    property var jobs: []
    // The Epic games being uninstalled: quitting would leave their files half deleted.
    property var uninstalls: []

    // Quit was pressed and Legendary's downloads are being stopped; the window closes when they are.
    property bool stopping: false

    signal quitConfirmed()

    function ask(list, jobList, uninstallList) {
        titles = list
        jobs = jobList ?? []
        uninstalls = uninstallList ?? []
        open()
    }

    carded: false
    title: qsTr("Quit Lattis?")
    closePolicy: stopping ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: stayButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        visible: root.stopping
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: qsTr("Stopping downloads…")
    }

    Label {
        Layout.fillWidth: true
        visible: !root.stopping && root.titles.length > 0
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.titles.length === 1
              ? qsTr("%1 is still running. It keeps running if you quit, but its play time isn't recorded.")
                    .arg(root.titles[0])
              : qsTr("%1 are still running. They keep running if you quit, but their play time isn't recorded.")
                    .arg(root.titles.join(", "))
    }

    Label {
        Layout.fillWidth: true
        visible: !root.stopping && root.jobs.length > 0
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.jobs.length === 1
              ? qsTr("%1 is being updated. Quitting stops the update; press Update again to resume it.")
                    .arg(root.jobs[0])
              : qsTr("%1 are being updated. Quitting stops the updates; press Update again to resume them.")
                    .arg(root.jobs.join(", "))
    }

    Label {
        Layout.fillWidth: true
        visible: !root.stopping && root.uninstalls.length > 0
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.warningColor
        text: root.uninstalls.length === 1
              ? qsTr("%1 is being uninstalled. Quitting now stops it halfway, with part of its files deleted.")
                    .arg(root.uninstalls[0])
              : qsTr("%1 are being uninstalled. Quitting now stops them halfway, with part of their files deleted.")
                    .arg(root.uninstalls.join(", "))
    }

    footer: [
        SettingsButton {
            id: stayButton
            visible: !root.stopping
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            visible: !root.stopping
            text: qsTr("Quit")
            danger: true
            onClicked: root.quitConfirmed()
        }
    ]
}
