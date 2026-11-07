import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})

    readonly property var rows: overview.attention ?? []
    readonly property int itemCount: rows.reduce((sum, r) => sum + r.count, 0)

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

    function iconOf(kind: string): string {
        switch (kind) {
        case "neverBackedUp": return Icons.save2Line
        case "missingArt": return Icons.image
        case "missingInfo": return Icons.informationLine
        case "updates": return Icons.download
        case "romsNotFound": return Icons.link
        case "emulatorsMissing": return Icons.gamepadLine
        }
        return ""
    }

    RowLayout {
        Layout.fillWidth: true

        OverviewCardTitle {
            Layout.fillWidth: true
            text: qsTr("Needs attention")
        }
        OverviewQuiet {
            visible: root.itemCount > 0
            text: qsTr("%n item(s)", "", root.itemCount)
        }
    }

    Repeater {
        model: root.rows

        RowLayout {
            id: line
            required property var modelData

            Layout.fillWidth: true
            spacing: Theme.layout.spacing * 1.5

            Rectangle {
                visible: Theme.overview.attentionIconTile > 0
                Layout.preferredWidth: Theme.overview.attentionIconTile
                Layout.preferredHeight: Theme.overview.attentionIconTile
                radius: Theme.radius.small
                color: Theme.overview.attentionIconBackground

                Icon {
                    anchors.centerIn: parent
                    size: Theme.overview.attentionIconSize
                    color: Theme.overview.attentionIcon
                    path: root.iconOf(line.modelData.kind)
                }
            }
            Label {
                Layout.preferredWidth: Theme.overview.attentionCountWidth
                text: line.modelData.count
                color: Theme.palette.text
                font.pixelSize: Theme.overview.attentionCountSize
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
