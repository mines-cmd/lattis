import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property var info
    property bool stacked: false
    property string mode: "largest"
    // Set once the bars have grown, so a refresh while the page is open doesn't grow them again.
    property bool barsGrown: false

    readonly property var drives: overview.drives ?? []
    readonly property var platformSizes: overview.platformSizes ?? []
    readonly property var rows: (mode === "largest" ? overview.largest : overview.dormant) ?? []
    readonly property bool hasContent: drives.length > 0 || (overview.largest ?? []).length > 0 || (overview.dormant ?? []).length > 0
    readonly property real otherTotal: drives.reduce((sum, d) => sum + d.otherBytes, 0)

    signal gameRequested(string id)

    onOverviewChanged: if (overview.drives === undefined) barsGrown = false

    function colorOf(platform: string): color {
        return info.colorOf(platform)
    }

    function nameOf(platform: string): string {
        return info.nameOf(platform)
    }

    function percent(part: real, whole: real): string {
        return whole > 0 ? qsTr("%1%").arg(Math.round(100 * part / whole)) : ""
    }

    OverviewCardTitle { text: qsTr("Storage") }

    GridLayout {
        Layout.fillWidth: true
        columns: root.stacked ? 1 : 2
        columnSpacing: 2 * Theme.overview.sectionSpacing
        rowSpacing: Theme.overview.sectionSpacing

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: Theme.overview.sectionSpacing
            visible: root.drives.length > 0

            Repeater {
                model: root.drives

                ColumnLayout {
                    id: drive
                    required property var modelData
                    property string hover: ""
                    property real grow: root.barsGrown ? 1 : 0

                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Component.onCompleted: {
                        grow = 1
                        root.barsGrown = true
                    }
                    Behavior on grow { enabled: Motion.on; MotionNumber { speed: "slow" } }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing

                        Label {
                            text: drive.modelData.name
                            color: Theme.palette.text
                            font.weight: Theme.font.emphasisWeight
                            elide: Text.ElideRight
                            Layout.maximumWidth: drive.width / 2
                        }
                        OverviewQuiet {
                            visible: drive.modelData.root !== drive.modelData.name
                            text: drive.modelData.root
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Item { Layout.fillWidth: drive.modelData.root === drive.modelData.name }
                        OverviewQuiet {
                            text: drive.hover !== "" ? drive.hover
                                : qsTr("%1 used of %2 · %3 free").arg(Library.sizeText(drive.modelData.total - drive.modelData.free))
                                    .arg(Library.sizeText(drive.modelData.total)).arg(Library.sizeText(drive.modelData.free))
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Rectangle {
                        id: track
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.overview.storageBarHeight
                        radius: Theme.overview.storageBarRadius
                        color: Theme.overview.storageFree

                        Rectangle {
                            id: barMask
                            anchors.fill: parent
                            radius: track.radius
                            visible: false
                            layer.enabled: true
                        }

                        Row {
                            anchors.fill: parent
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                maskEnabled: true
                                maskSource: barMask
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                            }
                            spacing: Theme.overview.storageSegmentGap

                            Repeater {
                                model: drive.modelData.platforms.concat([{ platform: "", bytes: drive.modelData.otherBytes }])

                                Rectangle {
                                    id: segment
                                    required property var modelData
                                    readonly property bool other: modelData.platform === ""

                                    visible: modelData.bytes > 0
                                    width: Math.max(Theme.overview.storageSegmentMinWidth * drive.grow * root.regrow, track.width * drive.grow * root.regrow * modelData.bytes / drive.modelData.total)
                                    height: track.height
                                    color: other ?Theme.overview.storageOther : root.colorOf(modelData.platform)

                                    HoverHandler {
                                        onHoveredChanged: {
                                            if (hovered) {
                                                const name = segment.other ? qsTr("Other files") : root.nameOf(segment.modelData.platform)
                                                drive.hover = qsTr("%1 · %2 · %3").arg(name).arg(Library.sizeText(segment.modelData.bytes))
                                                    .arg(root.percent(segment.modelData.bytes, drive.modelData.total))
                                            } else {
                                                drive.hover = ""
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    OverviewQuiet {
                        visible: drive.modelData.unknown > 0
                        text: qsTr("%n game(s) of unknown size aren't counted", "", drive.modelData.unknown)
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing * 2
                visible: root.platformSizes.length > 0

                Repeater {
                    model: root.platformSizes.concat(root.otherTotal > 0 ? [{ platform: "", bytes: root.otherTotal }] : [])

                    Row {
                        id: legend
                        required property var modelData
                        readonly property bool other: modelData.platform === ""
                        spacing: Theme.layout.spacing

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.font.smallSize
                            height: width
                            radius: Theme.overview.storageBarRadius
                            color: legend.other ? Theme.overview.storageOther : root.colorOf(legend.modelData.platform)
                        }
                        OverviewQuiet {
                            text: qsTr("%1 %2").arg(legend.other ? qsTr("Other files") : root.nameOf(legend.modelData.platform))
                                .arg(legend.modelData.bytes > 0 ? Library.sizeText(legend.modelData.bytes) : qsTr("size unknown"))
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: Theme.layout.spacing

            SegmentedToggle {
                accessibleName: qsTr("List")
                choices: [{ id: "largest", text: qsTr("Largest") }, { id: "dormant", text: qsTr("Dormant") }]
                current: root.mode
                onPicked: id => root.mode = id
            }

            OverviewQuiet {
                visible: root.rows.length === 0
                text: root.mode === "largest" ? qsTr("No installed game has a known size yet")
                                              : qsTr("Every installed game was played in the last six months")
            }

            Repeater {
                model: root.rows

                Item {
                    id: entry
                    required property var modelData
                    readonly property bool unknownSize: modelData.bytes <= 0

                    Layout.fillWidth: true
                    implicitHeight: thumb.height

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
                        anchors.right: size.left
                        anchors.rightMargin: Theme.layout.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: entry.modelData.title
                            color: Theme.palette.text
                            font.weight: Theme.font.emphasisWeight
                            elide: Text.ElideRight
                        }
                        OverviewQuiet {
                            Layout.fillWidth: true
                            text: root.mode === "largest"
                                ? root.nameOf(entry.modelData.platform)
                                : entry.modelData.lastPlayedText !== "" ? qsTr("Played %1").arg(entry.modelData.lastPlayedText)
                                                                        : qsTr("Not played yet")
                            elide: Text.ElideRight
                        }
                    }

                    OverviewQuiet {
                        id: size
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.unknownSize ? qsTr("Size unknown") : Library.sizeText(entry.modelData.bytes)
                    }
                }
            }
        }
    }
}
