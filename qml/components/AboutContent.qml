import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property color backgroundColor
    property color labelColor
    property color linkColor
    property int logoHeight: Theme.about.logoHeight

    implicitHeight: col.implicitHeight

    readonly property var credits: [
        { label: qsTr("Built with"), links: [["Qt", "https://www.qt.io"],
                                             ["toml++", "https://marzer.github.io/tomlplusplus/"],
                                             ["libarchive", "https://www.libarchive.org"]] },
        { label: qsTr("Game info"), links: [["IGDB", "https://www.igdb.com"]] },
        { label: qsTr("Artwork"), links: [["SteamGridDB", "https://www.steamgriddb.com"]] },
        { label: qsTr("Icons"), links: [["Remix Icon", "https://remixicon.com"]].concat(LicensedIcons.credits) },
        { label: qsTr("Covers"), text: qsTr("Game covers © their respective owners.") },
    ]

    component Paragraph: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: root.labelColor
        textFormat: Text.StyledText
        linkColor: root.linkColor
        onLinkActivated: link => Qt.openUrlExternally(link)

        HoverHandler {
            enabled: parent.hoveredLink !== ""
            cursorShape: Qt.PointingHandCursor
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.about.divider
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.about.logoSpacing
            implicitHeight: wordmark.height

            Wordmark {
                id: wordmark
                logoHeight: root.logoHeight
                text: Theme.about.logoText
                background: root.backgroundColor
            }
            Label {
                x: wordmark.width + Theme.about.versionSpacing
                anchors.baseline: wordmark.top
                anchors.baselineOffset: wordmark.textBaseline
                text: qsTr("Version %1").arg(Qt.application.version)
                color: Theme.about.versionColor
                font.pixelSize: Theme.about.versionFontSize
            }
        }

        Paragraph {
            Layout.bottomMargin: Theme.about.textSpacing
            color: root.labelColor
            text: qsTr("A game launcher for Linux: emulated games, Steam and Proton in one library.")
        }

        Divider {
            Layout.bottomMargin: Theme.gameEditor.rowSpacing
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.about.columnSpacing

            Repeater {
                model: 2

                delegate: GridLayout {
                    id: column
                    required property int index
                    readonly property var credits: {
                        const half = Math.ceil(root.credits.length / 2)
                        return index === 0 ? root.credits.slice(0, half) : root.credits.slice(half)
                    }
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: index === 1
                    columns: 2
                    columnSpacing: Theme.about.labelSpacing
                    rowSpacing: Theme.about.rowSpacing

                    Repeater {
                        model: column.credits

                        delegate: Label {
                            required property var modelData
                            required property int index
                            Layout.row: index
                            Layout.column: 0
                            Layout.alignment: Qt.AlignTop
                            text: modelData.label
                            color: Theme.about.labelColor
                        }
                    }
                    Repeater {
                        model: column.credits

                        delegate: Paragraph {
                            required property var modelData
                            required property int index
                            Layout.row: index
                            Layout.column: 1
                            text: modelData.text ?? modelData.links.map(l => "<a href=\"" + l[1] + "\">" + l[0] + "</a>").join(" · ")
                        }
                    }
                }
            }
        }
    }
}
