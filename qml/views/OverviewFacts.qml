import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})

    readonly property var longest: overview.longest ?? null
    readonly property var waiting: overview.waiting ?? null
    readonly property var achievements: overview.achievements ?? null
    // Each fact: a name, the value on the right, what is under the name, and where a click goes.
    readonly property var facts: {
        const out = []
        if (longest)
            out.push({ label: qsTr("Longest session"), value: Library.shortPlayTime(longest.minutes), game: longest.title,
                       detail: longest.endText, gameId: longest.id })
        if (waiting)
            out.push({ label: qsTr("Waiting longest"), value: waiting.waitingText, game: waiting.title, detail: "",
                       gameId: waiting.id })
        if (achievements && achievements.perfect > 0)
            out.push({ label: qsTr("Perfect games"), value: String(achievements.perfect), game: "", detail: qsTr("Steam games"),
                       gameId: "", target: "steam" })
        return out
    }

    signal gameRequested(string id)
    signal steamRequested()

    visible: facts.length > 0

    OverviewCardTitle { text: qsTr("Fun facts") }

    Repeater {
        model: root.facts

        Item {
            id: fact
            required property var modelData
            readonly property bool clickable: modelData.gameId !== "" || modelData.target !== undefined

            Layout.fillWidth: true
            implicitHeight: row.implicitHeight

            RowLayout {
                id: row
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Theme.layout.spacing * 1.5

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: fact.modelData.label
                        color: Theme.palette.text
                        elide: Text.ElideRight
                    }
                    OverviewQuiet {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: fact.modelData.game !== "" && fact.modelData.detail !== ""
                              ? qsTr("%1 · %2").arg(fact.modelData.game).arg(fact.modelData.detail)
                              : fact.modelData.game !== "" ? fact.modelData.game : fact.modelData.detail
                        elide: Text.ElideRight
                    }
                }
                PlainLabel {
                    text: fact.modelData.value
                    color: Theme.palette.text
                    font.weight: Theme.font.strongWeight
                }
            }

            HoverHandler { enabled: fact.clickable; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                enabled: fact.clickable
                onTapped: fact.modelData.gameId !== "" ? root.gameRequested(fact.modelData.gameId) : root.steamRequested()
            }
        }
    }
}
