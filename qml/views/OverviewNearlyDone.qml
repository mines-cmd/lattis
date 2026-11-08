import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property bool barsGrown: false

    readonly property var rows: overview.nearlyDone ?? []

    signal gameRequested(string id)

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    RowLayout {
        Layout.fillWidth: true

        OverviewCardTitle {
            Layout.fillWidth: true
            text: qsTr("Nearly done")
        }
        OverviewQuiet { text: qsTr("Games with 80%+ achievements") }
    }

    Repeater {
        model: root.rows

        Item {
            id: entry
            required property var modelData
            property real grow: root.barsGrown ? 1 : 0

            Layout.fillWidth: true
            implicitHeight: thumb.height

            Component.onCompleted: {
                grow = 1
                root.barsGrown = true
            }
            Behavior on grow { enabled: Motion.on; MotionNumber { speed: "slow" } }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.gameRequested(entry.modelData.id)
            }

            ArtThumb {
                id: thumb
                width: Theme.overview.storageThumbWidth
                height: Math.round(width * Theme.overview.coverRatio)
                cornerRadius: Theme.overview.coverRadius
                color: Theme.overview.coverPlaceholder
                iconWhenEmpty: true
                source: entry.modelData.coverUrl
            }

            ColumnLayout {
                anchors.left: thumb.right
                anchors.leftMargin: Theme.layout.spacing * 1.5
                anchors.right: progress.left
                anchors.rightMargin: Theme.layout.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: entry.modelData.title
                    color: Theme.palette.text
                    font.weight: Theme.font.emphasisWeight
                    elide: Text.ElideRight
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.overview.barHeight
                    radius: Theme.overview.barRadius
                    color: Theme.overview.barTrack

                    Rectangle {
                        width: Math.max(Theme.overview.barHeight, parent.width * entry.modelData.unlocked / entry.modelData.total * entry.grow * root.regrow)
                        height: parent.height
                        radius: parent.radius
                        color: Theme.overview.barFill
                    }
                }
            }

            OverviewQuiet {
                id: progress
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 of %2").arg(entry.modelData.unlocked).arg(entry.modelData.total)
            }
        }
    }
}
