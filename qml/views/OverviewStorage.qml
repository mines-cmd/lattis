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
    // Set once the bars have grown, so a refresh while the page is open doesn't grow them again.
    property bool barsGrown: false

    readonly property var drives: overview.drives ?? []
    readonly property var rows: overview.largest ?? []
    readonly property bool hasContent: drives.length > 0 || rows.length > 0
    readonly property real largestBytes: rows.reduce((most, r) => Math.max(most, r.bytes), 0)

    signal gameRequested(string id)
    signal allBySizeRequested()

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

    function roundSize(bytes: real): string {
        return Library.sizeText(bytes, 0)
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
                    property var hover: null
                    property real grow: root.barsGrown ? 1 : 0

                    readonly property real foldBelow: modelData.total * Theme.overview.storageFoldPercent / 100
                    readonly property var shown: modelData.platforms.filter(p => p.bytes > 0 && p.bytes >= foldBelow)
                    readonly property var folded: modelData.platforms.filter(p => p.bytes > 0 && p.bytes < foldBelow)
                    readonly property real otherBytes: folded.reduce((sum, p) => sum + p.bytes, modelData.otherBytes)
                    readonly property var parts: shown.concat(folded.length > 0 || (otherBytes > 0 && otherBytes >= foldBelow) ? [{ platform: "", bytes: otherBytes }] : [])

                    function partName(part: var): string {
                        return part.platform === "" ? qsTr("Other") : root.nameOf(part.platform)
                    }

                    function foldedText(part: var): string {
                        if (part.platform !== "" || folded.length === 0)
                            return ""
                        return qsTr("Includes %1").arg(folded.map(p => root.nameOf(p.platform)).join(", "))
                    }

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

                        PlainLabel {
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
                        RowLayout {
                            spacing: Theme.layout.spacing / 2

                            Rectangle {
                                visible: drive.hover !== null
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: Theme.font.smallSize
                                implicitHeight: implicitWidth
                                radius: Theme.overview.storageBarRadius
                                color: !drive.hover || drive.hover.platform === "" ? Theme.overview.storageOther : root.colorOf(drive.hover.platform)
                            }
                            Label {
                                visible: drive.hover !== null
                                Layout.alignment: Qt.AlignBaseline
                                text: drive.hover ? drive.partName(drive.hover) : ""
                                color: Theme.palette.text
                            }
                            Label {
                                Layout.alignment: Qt.AlignBaseline
                                text: drive.hover ? root.roundSize(drive.hover.bytes)
                                                  : qsTr("%1 free").arg(root.roundSize(drive.modelData.free))
                                color: Theme.palette.text
                                font.weight: Theme.font.emphasisWeight
                            }
                            OverviewQuiet {
                                Layout.alignment: Qt.AlignBaseline
                                text: drive.hover ? root.percent(drive.hover.bytes, drive.modelData.total)
                                                  : qsTr("of %1").arg(root.roundSize(drive.modelData.total))
                            }
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

                        // Clearing on the bar rather than each segment keeps the last part shown across the gaps.
                        HoverHandler {
                            onHoveredChanged: if (!hovered) drive.hover = null
                            onPointChanged: if (point.position.x >= segments.childrenRect.width) drive.hover = null
                        }

                        Row {
                            id: segments
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
                                model: drive.parts

                                Rectangle {
                                    id: segment
                                    required property var modelData

                                    visible: modelData.bytes > 0
                                    width: Math.max(Theme.overview.storageSegmentMinWidth * drive.grow * root.regrow, track.width * drive.grow * root.regrow * modelData.bytes / drive.modelData.total)
                                    height: track.height
                                    color: modelData.platform === "" ? Theme.overview.storageOther : root.colorOf(modelData.platform)

                                    HoverHandler {
                                        id: segmentHover
                                        onHoveredChanged: if (hovered) drive.hover = segment.modelData
                                    }
                                    ToolTip.visible: segmentHover.hovered && ToolTip.text !== ""
                                    ToolTip.delay: Theme.tooltip.delay
                                    ToolTip.text: drive.foldedText(segment.modelData)
                                }
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing / 2

                        HoverHandler {
                            onHoveredChanged: if (!hovered) drive.hover = null
                        }

                        Repeater {
                            model: drive.parts

                            Row {
                                id: legend
                                required property var modelData
                                spacing: Theme.layout.spacing
                                rightPadding: Theme.layout.spacing * 1.5

                                HoverHandler {
                                    id: legendHover
                                    onHoveredChanged: if (hovered) drive.hover = legend.modelData
                                }
                                ToolTip.visible: legendHover.hovered && ToolTip.text !== ""
                                ToolTip.delay: Theme.tooltip.delay
                                ToolTip.text: drive.foldedText(legend.modelData)

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Theme.font.smallSize
                                    height: width
                                    radius: Theme.overview.storageBarRadius
                                    color: legend.modelData.platform === "" ? Theme.overview.storageOther : root.colorOf(legend.modelData.platform)
                                }
                                Label {
                                    text: drive.partName(legend.modelData)
                                    color: Theme.palette.text
                                    font.pixelSize: Theme.font.smallSize
                                }
                                OverviewQuiet {
                                    text: root.roundSize(legend.modelData.bytes)
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
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: Theme.layout.spacing

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: root.stacked && root.drives.length > 0 ? Theme.overview.sectionSpacing : 0
                spacing: Theme.layout.spacing

                OverviewQuiet {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBaseline
                    text: qsTr("Largest games")
                    font.pixelSize: Theme.font.size
                    font.weight: Theme.font.emphasisWeight
                    elide: Text.ElideRight
                }
                TextLink {
                    Layout.alignment: Qt.AlignBaseline
                    Layout.rightMargin: -rightPadding
                    visible: root.rows.length > 0
                    text: qsTr("Show all games by size")
                    onClicked: root.allBySizeRequested()
                }
            }

            OverviewQuiet {
                visible: root.rows.length === 0
                text: qsTr("No installed game has a known size yet")
            }

            Repeater {
                model: root.rows

                Item {
                    id: entry
                    required property var modelData
                    readonly property bool unknownSize: modelData.bytes <= 0

                    Layout.fillWidth: true
                    Layout.topMargin: Theme.layout.spacing / 2
                    Layout.bottomMargin: Theme.layout.spacing / 2
                    implicitHeight: thumb.height

                    Rectangle {
                        anchors.fill: parent
                        anchors.topMargin: -Theme.layout.spacing / 2
                        anchors.bottomMargin: -Theme.layout.spacing / 2
                        anchors.leftMargin: -Theme.layout.spacing * 1.5
                        anchors.rightMargin: -Theme.layout.spacing * 1.5
                        radius: Theme.overview.storageRowRadius
                        color: Theme.palette.hoverOverlay
                        opacity: rowArea.containsMouse ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    }

                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
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
                    Rectangle {
                        anchors.fill: thumb
                        radius: thumb.radius
                        color: "transparent"
                        border.width: Theme.layout.borderWidth
                        border.color: Theme.overview.storageCoverOutline
                    }

                    ColumnLayout {
                        anchors.left: thumb.right
                        anchors.leftMargin: Theme.layout.spacing * 1.5
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            PlainLabel {
                                Layout.fillWidth: true
                                text: entry.modelData.title
                                color: Theme.palette.text
                                font.weight: Theme.font.emphasisWeight
                                elide: Text.ElideRight
                            }
                            Label {
                                text: entry.unknownSize ? qsTr("Size unknown") : Library.sizeText(entry.modelData.bytes)
                                color: entry.unknownSize ? Theme.overview.textMuted : Theme.palette.text
                                font.weight: entry.unknownSize ? Font.Normal : Theme.font.emphasisWeight
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            OverviewQuiet {
                                Layout.fillWidth: true
                                text: root.nameOf(entry.modelData.platform)
                                elide: Text.ElideRight
                            }
                            OverviewQuiet {
                                text: entry.modelData.lastPlayedText !== "" ? entry.modelData.lastPlayedText : qsTr("Not played yet")
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.layout.spacing
                            Layout.preferredHeight: Theme.overview.storageSizeBarHeight
                            visible: Theme.overview.storageSizeBarHeight > 0 && !entry.unknownSize
                            radius: height / 2
                            color: Theme.overview.storageFree

                            Rectangle {
                                width: root.largestBytes > 0 ? parent.width * entry.modelData.bytes / root.largestBytes : 0
                                height: parent.height
                                radius: parent.radius
                                color: root.colorOf(entry.modelData.platform)
                            }
                        }
                    }
                }
            }
        }
    }
}
