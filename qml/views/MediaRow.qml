import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property var game: ({})
    property var clips: []
    property bool installDismissed: false
    readonly property bool installing: FfmpegTool.state !== "idle" && FfmpegTool.state !== "failed"
    property bool active: true
    readonly property bool wanted: active && General.showClips && (game.platform ?? "") === "steam" && (game.id ?? "") !== ""

    signal clipRequested(var clip)
    signal menuRequested(var clip, Item tile, point position)

    visible: wanted && clips.length > 0
    spacing: 10

    function reload() {
        if (wanted)
            Library.media(game.id)
        else
            clips = []
    }

    onGameChanged: reload()
    onWantedChanged: reload()
    Component.onCompleted: reload()

    Connections {
        target: Library
        function onMediaReady(id, list) {
            if (id === root.game.id)
                root.clips = root.wanted ? list : []
        }
    }

    Label {
        text: qsTr("Media")
        color: Theme.details.headingColor
        font.weight: Theme.font.strongWeight
        font.pixelSize: Theme.details.headingFontSize
    }

    FfmpegPrompt {
        visible: !FfmpegTool.available && (!root.installDismissed || root.installing)
        Layout.fillWidth: true
        onNotNow: root.installDismissed = true
    }

    Flickable {
        id: flick
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.media.thumbHeight
                                + (scrollBar.policy === ScrollBar.AlwaysOn ? Theme.media.scrollbarGap + scrollBar.height : 0)
        contentWidth: tiles.width
        contentHeight: Theme.media.thumbHeight
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar {
            id: scrollBar
            policy: flick.contentWidth > flick.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        Row {
            id: tiles
            spacing: Theme.media.thumbGap

            Repeater {
                model: root.clips

                ArtThumb {
                    id: tile
                    required property var modelData
                    readonly property bool hovered: area.containsMouse

                    aspect: 16 / 9
                    cornerRadius: Theme.media.thumbRadius
                    implicitHeight: Theme.media.thumbHeight
                    source: modelData.thumbnail

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Theme.palette.hoverOverlay
                        opacity: tile.hovered ? 1 : 0
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "transparent"
                        border.color: Theme.media.thumbBorderColor
                        border.width: Theme.layout.borderWidth
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: Theme.media.playSize
                        height: width
                        radius: width / 2
                        color: Theme.media.badgeBackground

                        Icon {
                            anchors.centerIn: parent
                            path: Icons.playFill
                            size: parent.width / 2
                            color: Theme.media.playIconColor
                        }
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Theme.layout.spacing
                        width: lengthLabel.implicitWidth + Theme.layout.spacing * 2
                        height: lengthLabel.implicitHeight + Theme.layout.spacing / 2
                        radius: Theme.radius.tiny
                        color: Theme.media.badgeBackground

                        Label {
                            id: lengthLabel
                            anchors.centerIn: parent
                            text: tile.modelData.length
                            color: Theme.media.badgeText
                            font.pixelSize: Theme.details.headingFontSize
                        }
                    }

                    MouseArea {
                        id: area
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton)
                                root.menuRequested(tile.modelData, tile, Qt.point(mouse.x, mouse.y))
                            else
                                root.clipRequested(tile.modelData)
                        }
                        ToolTip.visible: containsMouse
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: tile.modelData.time.toLocaleDateString(Qt.locale(), General.shortDateFormat)
                    }
                }
            }
        }
    }
}
