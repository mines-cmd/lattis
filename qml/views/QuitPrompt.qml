import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var titles: []
    // The Epic games Legendary is updating or has queued: quitting stops them, and Update resumes them.
    property var jobs: []

    signal quitConfirmed()

    function ask(list, jobList) {
        titles = list
        jobs = jobList ?? []
        open()
    }

    carded: false
    title: qsTr("Quit Lattis?")

    onOpened: stayButton.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        visible: root.titles.length > 0
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
        visible: root.jobs.length > 0
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: root.jobs.length === 1
              ? qsTr("%1 is being updated. Quitting stops the update; press Update again to resume it.")
                    .arg(root.jobs[0])
              : qsTr("%1 are being updated. Quitting stops the updates; press Update again to resume them.")
                    .arg(root.jobs.join(", "))
    }

    footer: [
        SettingsButton {
            id: stayButton
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            text: qsTr("Quit")
            danger: true
            onClicked: root.quitConfirmed()
        }
    ]
}
