import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
FocusScope {
    id: root

    property real dragReserve: 0
    property var overview: ({})
    readonly property bool loaded: overview.games !== undefined

    // Three columns of cards, two below compactWidth, one below narrowWidth.
    readonly property int columns: Theme.overview.compactWidth > 0 && width < Theme.overview.compactWidth
        ? (Theme.overview.narrowWidth > 0 && width < Theme.overview.narrowWidth ? 1 : 2) : 3
    readonly property real sideMargin: 2 * Theme.layout.margin
    readonly property real contentWidth: Math.min(Theme.overview.maxWidth, width - 2 * sideMargin)
    readonly property var continuePlaying: overview.continuePlaying ?? []

    signal closeRequested()
    signal filterRequested(string platform, var statuses, var tags)
    signal gameRequested(string id)
    signal playRequested(string id)
    signal statsRequested()
    signal uninstallRequested(string id)

    function start() {
        overview = {}
        Library.overview()
        sinkOut.stop()
        riseIn.restart()
        forceActiveFocus()
    }

    function leave() {
        riseIn.stop()
        sinkOut.restart()
    }

    function hoursText(minutes: int): string {
        return minutes <= 0 ? qsTr("0 min") : Library.shortPlayTime(minutes)
    }

    function daysText(minutes: int): string {
        return minutes < 1440 ? qsTr("Less than a day") : qsTr("%1 days").arg((minutes / 1440).toFixed(1))
    }

    function openTile(target: string) {
        if (target === "stats")
            statsRequested()
        else
            filterRequested("", target === "backlog" ? ["not_started"] : [], [])
    }

    readonly property var tiles: {
        const o = overview
        if (!loaded)
            return []
        return [
            { label: qsTr("Games"), value: String(o.games), icon: Icons.stackLine, color: Theme.palette.text,
              sub: qsTr("%1 installed on %n platform(s)", "", o.platforms).arg(o.installed), target: "all" },
            { label: qsTr("Hours played"), value: hoursText(o.minutes), icon: Icons.timeLine, color: Theme.overview.tilePrimaryValue,
              sub: daysText(o.minutes), target: "stats" },
            { label: qsTr("Backlog"), value: String(o.backlog), icon: Icons.calendarCheckLine, color: Theme.palette.text,
              sub: o.games > 0 ? qsTr("%1% of the library").arg(Math.round(100 * o.backlog / o.games)) : qsTr("Nothing in the library"), target: "backlog" }
        ]
    }

    Keys.onEscapePressed: root.closeRequested()
    Keys.onPressed: event => {
        const page = flick.height - Theme.layout.margin
        const line = Theme.overview.sectionSpacing * 4
        const steps = { [Qt.Key_Down]: line, [Qt.Key_Up]: -line, [Qt.Key_PageDown]: page, [Qt.Key_PageUp]: -page }
        const top = Math.max(0, flick.contentHeight - flick.height)
        if (event.key === Qt.Key_Home)
            flick.contentY = 0
        else if (event.key === Qt.Key_End)
            flick.contentY = top
        else if (event.key in steps)
            flick.contentY = Math.max(0, Math.min(top, flick.contentY + steps[event.key]))
        else
            return
        event.accepted = true
    }

    OverviewPlatformInfo {
        id: platformInfo
        platformList: root.overview.platformList ?? []
    }

    Connections {
        target: Library
        function onOverviewReady(result) { root.overview = result }
        function onGamesChanged() {
            if (root.visible && root.loaded)
                Library.overview()
        }
    }

    MotionNumber {
        id: riseIn
        target: rise
        property: "y"
        from: Theme.overview.pageOffset
        to: 0
        speed: "slow"
    }

    // Not riseIn reversed: that saves its movement for the end, when the page has all but faded.
    MotionNumber {
        id: sinkOut
        target: rise
        property: "y"
        to: Theme.overview.pageOffset
        speed: "normal"
    }

    Surface {
        anchors.fill: parent
        fill: Theme.palette.background
        fillEnd: Theme.palette.backgroundEnd
        direction: Theme.palette.gradientDirection
    }

    Item {
        width: parent.width - root.dragReserve
        height: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing

        DragHandler {
            target: null
            enabled: !(root.Window.window?.toolbarHovered ?? false)
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }

    ChromeButton {
        x: Theme.layout.margin
        y: Theme.layout.margin
        icon: Icons.arrowUp
        iconRotation: -90
        label: qsTr("Back to games")
        tip: qsTr("Back to games (Esc)")
        onClicked: root.closeRequested()
    }

    Flickable {
        id: flick

        y: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing
        width: parent.width
        height: parent.height - y
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * root.sideMargin
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            transform: Translate { id: rise }

            x: (flick.width - width) / 2
            width: root.contentWidth
            spacing: Theme.overview.sectionSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: 2 * Theme.overview.sectionSpacing

                Label {
                    text: qsTr("Library overview")
                    color: Theme.palette.text
                    font.pixelSize: Theme.overview.titleSize
                    font.weight: Theme.font.emphasisWeight
                }
                Item { Layout.fillWidth: true }
                ChromeButton {
                    icon: Icons.barChartFill
                    label: qsTr("Play statistics")
                    tip: qsTr("Play statistics")
                    onClicked: root.statsRequested()
                }
            }

            ColumnLayout {
                visible: root.loaded
                Layout.fillWidth: true
                spacing: Theme.overview.sectionSpacing

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.columns === 3 ? Math.max(1, root.tiles.length) : root.columns === 2 ? 2 : 1
                    columnSpacing: Theme.overview.sectionSpacing
                    rowSpacing: Theme.overview.sectionSpacing
                    uniformCellWidths: true

                    Repeater {
                        model: root.tiles

                        OverviewReveal {
                            id: tileReveal
                            shown: root.loaded
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            order: index

                            OverviewCard {
                                Layout.fillWidth: true
                                horizontalPadding: Theme.overview.tilePaddingX
                                watermark: tileReveal.modelData.icon

                                TapHandler { onTapped: root.openTile(tileReveal.modelData.target) }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }

                                CardLabel {
                                    text: tileReveal.modelData.label
                                    color: Theme.overview.textMuted
                                }
                                Label {
                                    text: tileReveal.modelData.value
                                    color: tileReveal.modelData.color
                                    font.pixelSize: Theme.overview.tileValueSize
                                    font.weight: Theme.font.strongWeight
                                }
                                OverviewQuiet {
                                    Layout.fillWidth: true
                                    text: tileReveal.modelData.sub
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                OverviewReveal {
                    shown: root.loaded
                    Layout.fillWidth: true
                    visible: root.continuePlaying.length > 0
                    order: root.tiles.length

                    OverviewCard {
                        Layout.fillWidth: true

                        OverviewCardTitle { text: qsTr("Continue playing") }

                        Flickable {
                            id: coverStrip
                            Layout.fillWidth: true
                            Layout.preferredHeight: coverRow.implicitHeight
                            contentWidth: coverRow.implicitWidth
                            contentHeight: coverRow.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.HorizontalFlick
                            ScrollBar.horizontal: ScrollBar { visible: coverStrip.contentWidth > coverStrip.width }

                            Row {
                                id: coverRow
                                spacing: Theme.overview.coverSpacing

                                Repeater {
                                    model: root.continuePlaying

                                    Item {
                                        id: entry
                                        required property var modelData
                                        readonly property bool hovered: hover.hovered

                                        width: Theme.overview.coverWidth
                                        implicitHeight: cover.height + caption.implicitHeight + Theme.layout.spacing

                                        ArtThumb {
                                            id: cover
                                            width: parent.width
                                            height: Math.round(width * Theme.overview.coverRatio)
                                            cornerRadius: Theme.overview.coverRadius
                                            color: Theme.overview.coverPlaceholder
                                            iconWhenEmpty: true
                                            source: entry.modelData.coverUrl

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.gameRequested(entry.modelData.id)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: cover.radius
                                                color: Theme.overview.coverHoverOverlay
                                                opacity: entry.hovered ? 1 : 0
                                                visible: opacity > 0
                                                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                                                ChromeButton {
                                                    anchors.centerIn: parent
                                                    icon: Icons.playFill
                                                    label: qsTr("Play")
                                                    onClicked: root.playRequested(entry.modelData.id)
                                                }
                                            }
                                        }

                                        HoverHandler { id: hover }

                                        Column {
                                            id: caption
                                            anchors.top: cover.bottom
                                            anchors.topMargin: Theme.layout.spacing
                                            width: parent.width

                                            Label {
                                                width: parent.width
                                                text: entry.modelData.title
                                                color: Theme.palette.text
                                                font.weight: Theme.font.emphasisWeight
                                                elide: Text.ElideRight
                                            }
                                            OverviewQuiet {
                                                width: parent.width
                                                text: entry.modelData.lastPlayedText !== "" ? entry.modelData.lastPlayedText
                                                                                            : qsTr("Not played yet")
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                OverviewReveal {
                    shown: root.loaded
                    Layout.fillWidth: true
                    visible: storage.hasContent
                    order: root.tiles.length + 1

                    OverviewStorage {
                        id: storage
                        Layout.fillWidth: true
                        overview: root.overview
                        info: platformInfo
                        stacked: root.columns === 1
                        onGameRequested: id => root.gameRequested(id)
                        onUninstallRequested: id => root.uninstallRequested(id)
                    }
                }

                OverviewReveal {
                    shown: root.loaded
                    Layout.fillWidth: true
                    visible: (root.overview.platformCounts ?? []).length > 0
                    order: root.tiles.length + 2

                    OverviewPlatforms {
                        Layout.fillWidth: true
                        overview: root.overview
                        info: platformInfo
                        onFilterRequested: platform => root.filterRequested(platform, [], [])
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.columns
                    columnSpacing: Theme.overview.sectionSpacing
                    rowSpacing: Theme.overview.sectionSpacing
                    uniformCellWidths: true

                    OverviewReveal {
                        shown: root.loaded
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        visible: (root.overview.statusCounts ?? []).length > 0
                        order: root.tiles.length + 3

                        OverviewStatuses {
                            Layout.fillWidth: true
                            overview: root.overview
                            onFilterRequested: status => root.filterRequested("", [status], [])
                        }
                    }

                    OverviewReveal {
                        shown: root.loaded
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        visible: (root.overview.topTags ?? []).length > 0
                        order: root.tiles.length + 4

                        OverviewTags {
                            Layout.fillWidth: true
                            overview: root.overview
                            onFilterRequested: tag => root.filterRequested("", [], [tag])
                        }
                    }

                    OverviewReveal {
                        shown: root.loaded
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        visible: (root.overview.years ?? []).length > 0
                        order: root.tiles.length + 5

                        OverviewYears {
                            Layout.fillWidth: true
                            overview: root.overview
                        }
                    }
                }
            }
        }
    }
}
