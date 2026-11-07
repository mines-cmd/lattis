import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property var info
    property string pickedId: ""

    readonly property var picks: overview.backlogPicks ?? []
    readonly property var pick: picks.find(p => p.id === pickedId) ?? null

    signal gameRequested(string id)
    signal playRequested(string id)

    function randomId(except: string): string {
        const pool = picks.filter(p => p.id !== except)
        return pool.length > 0 ? pool[Math.floor(Math.random() * pool.length)].id : except
    }

    // A refresh keeps the game on show while it is still in the backlog.
    onPicksChanged: {
        if (!picks.some(p => p.id === pickedId))
            pickedId = randomId("")
    }

    OverviewCardTitle { text: qsTr("From your backlog") }

    RowLayout {
        id: body
        Layout.fillWidth: true
        spacing: Theme.layout.spacing * 1.5

        ArtThumb {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Theme.overview.backlogCoverWidth
            Layout.preferredHeight: Math.round(Theme.overview.backlogCoverWidth * Theme.overview.coverRatio)
            cornerRadius: Theme.overview.coverRadius
            color: Theme.overview.coverPlaceholder
            iconWhenEmpty: true
            source: root.pick?.coverUrl ?? ""

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.pick) root.gameRequested(root.pick.id)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: root.pick?.title ?? ""
                color: Theme.palette.text
                font.weight: Theme.font.emphasisWeight
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            OverviewQuiet {
                Layout.fillWidth: true
                text: root.pick ? (root.pick.year > 0 ? qsTr("%1 · %2").arg(root.info.nameOf(root.pick.platform)).arg(root.pick.year)
                                                      : root.info.nameOf(root.pick.platform)) : ""
                elide: Text.ElideRight
            }

            Row {
                spacing: Theme.layout.spacing

                ChromeButton {
                    icon: Icons.playFill
                    label: qsTr("Play")
                    onClicked: if (root.pick) root.playRequested(root.pick.id)
                }
                ChromeButton {
                    icon: Icons.shuffle
                    label: qsTr("Shuffle")
                    enabled: root.picks.length > 1
                    onClicked: swap.restart()
                }
            }
        }
    }

    // Fades the game out, swaps it, and fades the next in: a crossfade that follows Reduce motion like every fade.
    SequentialAnimation {
        id: swap
        MotionNumber { target: body; property: "opacity"; to: 0; speed: "fast"; fade: true }
        ScriptAction { script: root.pickedId = root.randomId(root.pickedId) }
        MotionNumber { target: body; property: "opacity"; to: 1; speed: "fast"; fade: true }
    }
}
