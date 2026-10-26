import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property var game: ({})
    property var items: []
    property bool active: true
    property int settledCount: 0
    readonly property bool wanted: active && General.showAchievements && (game.platform ?? "") === "steam" && (game.id ?? "") !== ""
    readonly property int unlocked: items.filter(a => a.unlocked).length
    readonly property bool complete: items.length > 0 && unlocked === items.length
    readonly property int perRow: Math.max(1, Math.floor((icons.width + Theme.achievements.iconGap)
                                                         / (Theme.achievements.iconSize + Theme.achievements.iconGap)))
    readonly property int fitting: perRow * Theme.achievements.maxRows
    readonly property bool cut: items.length > fitting
    readonly property int shownCount: Math.min(items.length, fitting)

    readonly property bool shown: wanted && items.length > 0

    visible: shown
    implicitHeight: column.implicitHeight + 2 * Theme.achievements.padding
    radius: Theme.achievements.radius
    fill: Theme.achievements.background
    fillEnd: Theme.achievements.backgroundEnd
    direction: Theme.achievements.gradientDirection
    border.color: Theme.achievements.border
    border.width: Theme.achievements.borderWidth

    function reload() {
        if (wanted)
            Library.achievements(game.id)
        else
            items = []
    }

    function tip(a) {
        if (a.unlocked)
            return [a.name, a.description,
                    qsTr("Unlocked %1").arg(a.time.toLocaleDateString(Qt.locale(), General.shortDateFormat))]
                .filter(line => line !== "").join("\n")
        if (a.hidden)
            return qsTr("Hidden achievement") + "\n"
                   + qsTr("Details for this achievement will be revealed once you've unlocked it") + "\n" + qsTr("Locked")
        return [a.name, a.description, qsTr("Locked")].filter(line => line !== "").join("\n")
    }

    function openViewer() {
        viewer.active = true
        viewer.item.show(game, items)
    }

    onGameChanged: reload()
    onWantedChanged: reload()
    Component.onCompleted: reload()

    Loader {
        id: viewer
        active: false
        source: "AchievementsViewer.qml"
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        onTapped: root.openViewer()
    }

    Connections {
        target: Library
        function onAchievementsReady(id, list) {
            if (id === root.game.id)
                root.items = root.wanted ? list : []
        }
    }

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: Theme.achievements.padding
                  leftMargin: Theme.achievements.sidePadding; rightMargin: Theme.achievements.sidePadding }
        spacing: Theme.layout.spacing * 2

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: qsTr("Achievements")
                color: Theme.achievements.titleColor
                font.weight: Theme.achievements.titleWeight
            }
            Label {
                text: qsTr("%1 / %2").arg(root.unlocked).arg(root.items.length)
                color: root.complete ? Theme.achievements.completeColor : Theme.achievements.countColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.achievements.barHeight
            radius: height / 2
            color: Theme.achievements.barColor

            Rectangle {
                width: root.items.length > 0 ? parent.width * root.unlocked / root.items.length : 0
                height: parent.height
                radius: parent.radius
                color: Theme.achievements.barFill
                Behavior on width { enabled: Motion.on; MotionNumber {} }
            }
        }

        // Placed by hand: a Flow whose spacing follows its own width kept re-polishing itself.
        Item {
            id: icons
            Layout.fillWidth: true
            // iconGap is the least space; the rest of the row is shared out so both edges line up.
            readonly property real step: root.perRow > 1 ? (width - Theme.achievements.iconSize) / (root.perRow - 1)
                                                         : Theme.achievements.iconSize + Theme.achievements.iconGap
            readonly property int rows: Math.ceil(root.shownCount / root.perRow)
            implicitHeight: rows > 0 ? (rows - 1) * (Theme.achievements.iconSize + Theme.achievements.iconGap)
                                       + Theme.achievements.iconSize : 0

            Repeater {
                model: root.shownCount

                ArtThumb {
                    id: tile
                    required property int index
                    readonly property var modelData: root.items[index]
                    x: (index % root.perRow) * icons.step
                    y: Math.floor(index / root.perRow) * (Theme.achievements.iconSize + Theme.achievements.iconGap)
                    width: Theme.achievements.iconSize
                    height: Theme.achievements.iconSize

                    aspect: 1
                    cornerRadius: Theme.achievements.iconRadius
                    color: Theme.achievements.iconPlaceholder
                    implicitHeight: Theme.achievements.iconSize
                    // Steam's CDN answers 503 to a burst of requests, so icons start a few at a time and a
                    // failed one is tried again.
                    property int tries: 0
                    property bool counted: false
                    readonly property bool concealed: (modelData?.hidden ?? false) && !modelData.unlocked
                    readonly property bool settled: concealed || status === Image.Ready || (status === Image.Error && tries >= 5)
                    // Latched: a source that follows settledCount would loop through its own status.
                    property bool started: false
                    failedIcon: Icons.medal
                    iconWhenEmpty: !concealed
                    source: started && !concealed ? (modelData?.icon ?? "") : ""

                    Label {
                        visible: tile.concealed
                        anchors.centerIn: parent
                        text: "?"
                        color: Theme.achievements.countColor
                        font.pixelSize: parent.height * 0.5
                    }
                    Connections {
                        target: root
                        function onSettledCountChanged() { tile.start() }
                    }
                    Component.onCompleted: start()
                    function start() {
                        if (!started && index < root.settledCount + 3)
                            started = true
                    }
                    onSettledChanged: {
                        if (settled === counted)
                            return
                        counted = settled
                        root.settledCount += settled ? 1 : -1
                    }
                    Component.onDestruction: {
                        if (counted)
                            root.settledCount--
                    }
                    onStatusChanged: {
                        if (status === Image.Error && tries < 5)
                            retry.start()
                    }
                    Timer {
                        id: retry
                        interval: 1000 * (tile.tries + 1)
                        onTriggered: {
                            tile.tries++
                            tile.started = false
                            tile.started = true
                        }
                    }
                    opacity: modelData?.unlocked ? 1 : Theme.achievements.lockedOpacity

                    HoverHandler {
                        id: hover
                    }
                    ToolTip.visible: hover.hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: modelData ? root.tip(modelData) : ""
                }
            }
        }

        Label {
            visible: root.cut
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Show all (%1)").arg(root.items.length)
            color: moreArea.containsMouse ? Theme.achievements.linkHoverColor : Theme.achievements.linkColor
            Behavior on color { enabled: Motion.fades; MotionColor {} }

            MouseArea {
                id: moreArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openViewer()
            }
        }
    }
}
