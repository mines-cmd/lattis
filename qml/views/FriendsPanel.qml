import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property var game: ({})
    property bool active: true
    readonly property int appId: {
        const m = /^steam-(\d+)$/.exec(game.id ?? "")
        return m ? parseInt(m[1]) : 0
    }
    readonly property var groups: General.friendsGroups
    readonly property bool wanted: active && General.gamePagePanels.includes("friends") && appId > 0 && groups.length > 0

    property var activity: ({ playing: [], recent: [], previous: [] })
    readonly property var playing: groups.includes("playing") ? activity.playing : []
    // A friend in the game is listed once, under Playing now, while that group is shown.
    readonly property var playingIds: new Set(playing.map(p => p.steamId))
    readonly property var recent: groups.includes("recent") ? activity.recent.filter(p => !playingIds.has(p.steamId)) : []
    readonly property var previous: groups.includes("previous") ? activity.previous.filter(p => !playingIds.has(p.steamId)) : []
    readonly property var recentShown: recent.slice(0, Theme.friends.recentMax)
    readonly property int count: {
        const ids = new Set()
        for (const list of [playing, recent, previous])
            for (const p of list)
                ids.add(p.steamId)
        return ids.size
    }

    readonly property bool shown: wanted && count > 0

    readonly property string playingTitle: qsTr("Playing now")
    readonly property string recentTitle: qsTr("Played recently  %1").arg(recent.length)
    readonly property string previousTitle: qsTr("Played previously  %1").arg(previous.length)
    // The top group's heading sits in the header, beside View all, so the panel has one title.
    readonly property string topGroup: playing.length > 0 ? "playing" : recent.length > 0 ? "recent" : "previous"

    signal menuRequested(point position)

    visible: shown
    implicitHeight: column.implicitHeight + 2 * Theme.friends.padding
    radius: Theme.friends.radius
    fill: Theme.friends.background
    fillEnd: Theme.friends.backgroundEnd
    direction: Theme.friends.gradientDirection
    border.color: Theme.friends.border
    border.width: Theme.friends.borderWidth

    function reload() {
        activity = wanted ? FriendsActivity.forGame(appId) : { playing: [], recent: [], previous: [] }
    }

    function hours(minutes) {
        const total = minutes ?? 0
        if (General.playTimeFormat !== "hours" || total <= 0)
            return Library.playTime(total, General.playTimeFormat)
        if (total < 60)
            return qsTr("%1 min").arg(total)
        return qsTr("%1 hrs").arg(Qt.locale().toString(total / 60, "f", 1))
    }

    onAppIdChanged: reload()
    onWantedChanged: reload()
    Component.onCompleted: reload()

    Connections {
        target: FriendsActivity
        function onChanged() { root.reload() }
    }

    // Who is playing now is only asked for while the panel could show it; the singleton limits it to once a minute.
    Timer {
        interval: 60000
        repeat: true
        running: root.wanted
        triggeredOnStart: true
        onTriggered: FriendsActivity.refreshPlaying()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => root.menuRequested(point.position)
    }

    component Face: ArtThumb {
        id: face
        property var person
        property int faceSize: Theme.friends.avatarSize
        width: faceSize
        height: faceSize
        implicitHeight: faceSize
        aspect: 1
        cornerRadius: Theme.friends.avatarRadius
        color: Theme.friends.avatarPlaceholder
        source: person?.avatar ?? ""
    }

    component GroupHeading: Label {
        Layout.fillWidth: true
        color: Theme.friends.groupColor
        font.weight: Theme.friends.groupWeight
        elide: Text.ElideRight
    }

    component FriendRow: Item {
        id: row
        required property var modelData
        property bool playing: false
        Layout.fillWidth: true
        implicitHeight: Theme.friends.avatarSize + 2 * Theme.friends.rowPadding

        Rectangle {
            anchors.fill: parent
            radius: Theme.friends.rowRadius
            color: Theme.friends.rowHover
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
        }
        HoverHandler {
            id: hover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            onTapped: FriendsActivity.openProfile(row.modelData.steamId)
        }

        RowLayout {
            anchors { fill: parent; leftMargin: Theme.friends.rowPadding; rightMargin: Theme.friends.rowPadding }
            spacing: Theme.friends.nameGap

            Face { person: row.modelData }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: row.modelData.name
                    color: row.playing ? Theme.friends.playingColor
                           : row.modelData.online ? Theme.friends.onlineColor : Theme.friends.nameColor
                    font.pixelSize: Theme.friends.nameFontSize
                    font.weight: Theme.friends.nameWeight
                    elide: Text.ElideRight
                }
                Label {
                    visible: !row.playing
                    Layout.fillWidth: true
                    text: root.hours(row.modelData.recentMinutes)
                    color: Theme.friends.hoursColor
                    opacity: Theme.friends.hoursOpacity
                    font.pixelSize: Theme.friends.hoursFontSize
                    elide: Text.ElideRight
                }
                Label {
                    visible: row.playing && (row.modelData.gameName ?? "") !== ""
                    Layout.fillWidth: true
                    text: row.modelData.gameName ?? ""
                    color: Theme.friends.playingColor
                    font.pixelSize: Theme.friends.hoursFontSize
                    elide: Text.ElideRight
                }
            }
        }
    }

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: Theme.friends.padding
                  leftMargin: Theme.friends.sidePadding; rightMargin: Theme.friends.sidePadding }
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: root.topGroup === "playing" ? root.playingTitle
                      : root.topGroup === "recent" ? root.recentTitle : root.previousTitle
                color: Theme.friends.titleColor
                font.weight: Theme.friends.titleWeight
                elide: Text.ElideRight
            }
            Label {
                text: qsTr("View all")
                color: viewAll.containsMouse ? Theme.friends.linkHoverColor : Theme.friends.linkColor
                Behavior on color { enabled: Motion.fades; MotionColor {} }

                MouseArea {
                    id: viewAll
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: FriendsActivity.openFriendsWhoPlay(root.appId)
                }
            }
        }

        ColumnLayout {
            visible: root.playing.length > 0
            Layout.fillWidth: true
            Layout.topMargin: Theme.friends.rowGap
            spacing: Theme.friends.rowGap

            Repeater {
                model: root.playing
                FriendRow { playing: true }
            }
        }

        ColumnLayout {
            visible: root.recent.length > 0
            Layout.fillWidth: true
            Layout.topMargin: root.topGroup === "recent" ? Theme.friends.rowGap : Theme.friends.groupGap
            spacing: Theme.friends.rowGap

            GroupHeading {
                visible: root.topGroup !== "recent"
                text: root.recentTitle
            }
            Repeater {
                model: root.recentShown
                FriendRow {}
            }
        }

        ColumnLayout {
            visible: root.previous.length > 0
            Layout.fillWidth: true
            Layout.topMargin: root.topGroup === "previous" ? Theme.friends.rowGap : Theme.friends.groupGap
            spacing: Theme.friends.rowGap

            GroupHeading {
                visible: root.topGroup !== "previous"
                text: root.previousTitle
            }

            // Placed by hand, like the achievements' icons.
            Item {
                id: faces
                Layout.fillWidth: true
                readonly property int size: Theme.friends.previousSize
                readonly property int gap: Theme.friends.previousGap
                readonly property int perRow: Math.max(1, Math.floor((width + gap) / (size + gap)))
                readonly property int fitting: perRow * Theme.friends.previousMaxRows
                readonly property bool cut: root.previous.length > fitting
                // The last place holds the +N, so the faces stop one short.
                readonly property int shownCount: cut ? fitting - 1 : root.previous.length
                readonly property int rows: Math.ceil((shownCount + (cut ? 1 : 0)) / perRow)
                implicitHeight: rows > 0 ? (rows - 1) * (size + gap) + size : 0

                Repeater {
                    model: faces.shownCount

                    Face {
                        id: tile
                        required property int index
                        readonly property var friendData: root.previous[index]
                        person: friendData
                        faceSize: faces.size
                        x: (index % faces.perRow) * (faces.size + faces.gap)
                        y: Math.floor(index / faces.perRow) * (faces.size + faces.gap)

                        HoverHandler {
                            id: tileHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: FriendsActivity.openProfile(tile.friendData.steamId)
                        }
                        ToolTip.visible: tileHover.hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("%1\n%2 played").arg(friendData?.name ?? "").arg(root.hours(friendData?.totalMinutes))
                    }
                }
                Label {
                    visible: faces.cut
                    x: (faces.shownCount % faces.perRow) * (faces.size + faces.gap)
                    y: Math.floor(faces.shownCount / faces.perRow) * (faces.size + faces.gap)
                    width: faces.size
                    height: faces.size
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("+%1").arg(root.previous.length - faces.shownCount)
                    color: Theme.friends.moreColor
                }
            }
        }
    }
}
