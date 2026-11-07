import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})

    readonly property var rows: overview.attention ?? []

    signal actionRequested(string kind)

    function labelOf(kind: string): string {
        switch (kind) {
        case "neverBackedUp": return qsTr("Played games with saves never backed up")
        case "missingArt": return qsTr("Games without a cover")
        case "missingInfo": return qsTr("Games without game info")
        case "updates": return qsTr("Steam updates waiting")
        case "romsNotFound": return qsTr("ROMs not found")
        case "emulatorsMissing": return qsTr("Games whose emulator isn't set up")
        }
        return kind
    }

    function actionOf(kind: string): string {
        switch (kind) {
        case "neverBackedUp": return qsTr("Back up")
        case "missingArt":
        case "missingInfo": return qsTr("Look up")
        case "updates": return qsTr("Show")
        case "romsNotFound": return qsTr("Platforms")
        case "emulatorsMissing": return qsTr("Emulators")
        }
        return ""
    }

    OverviewCardTitle { text: qsTr("Needs attention") }

    Repeater {
        model: root.rows

        RowLayout {
            id: line
            required property var modelData

            Layout.fillWidth: true
            spacing: Theme.layout.spacing * 1.5

            Label {
                Layout.preferredWidth: Theme.overview.countWidth
                text: line.modelData.count
                color: Theme.palette.text
                font.weight: Theme.font.strongWeight
                horizontalAlignment: Text.AlignRight
            }
            Label {
                Layout.fillWidth: true
                text: root.labelOf(line.modelData.kind)
                color: Theme.palette.text
                elide: Text.ElideRight
            }
            SettingsButton {
                text: root.actionOf(line.modelData.kind)
                onClicked: root.actionRequested(line.modelData.kind)
            }
        }
    }
}
