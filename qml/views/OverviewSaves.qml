import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property bool barsGrown: false

    readonly property var stats: overview.saves ?? ({})
    readonly property int played: stats.played ?? 0

    signal backUpRequested()

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    function countText(): string {
        return qsTr("%1 of %2 · %3").arg(stats.backedUp).arg(played).arg(percentText(stats.backedUp, played))
    }

    OverviewQuiet {
        id: sample
        visible: false
        text: root.countText()
    }

    OverviewCardTitle { text: qsTr("Save backups") }

    OverviewBarRow {
        regrow: root.regrow
        visible: root.played > 0
        name: qsTr("Played games")
        countText: root.countText()
        countWidth: sample.implicitWidth
        fraction: root.played > 0 ? root.stats.backedUp / root.played : 0
        grown: root.barsGrown
        onGrew: root.barsGrown = true
    }

    Repeater {
        model: [
            { label: qsTr("Last backup"), value: root.stats.lastText ?? "" },
            { label: qsTr("Backups"), value: root.stats.archives > 0 ? String(root.stats.archives) : "" },
            { label: qsTr("Space used"), value: root.stats.archives > 0 ? Library.sizeText(root.stats.bytes) : "" }
        ]

        RowLayout {
            required property var modelData
            Layout.fillWidth: true
            visible: modelData.value !== ""

            OverviewQuiet {
                Layout.fillWidth: true
                text: parent.modelData.label
            }
            Label {
                text: parent.modelData.value
                color: Theme.palette.text
                font.weight: Theme.font.emphasisWeight
            }
        }
    }

    OverviewQuiet {
        visible: root.stats.archives === 0
        text: qsTr("Nothing backed up yet")
    }

    ChromeButton {
        Layout.alignment: Qt.AlignLeft
        icon: Icons.save2Line
        label: qsTr("Back up now")
        tip: qsTr("Back up saves")
        onClicked: root.backUpRequested()
    }
}
