import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
ColumnLayout {
    id: root

    property var game: ({})
    property var items: []
    readonly property var shown: items.filter(i => i.kind === "clip" ? General.showVideo : General.showScreenshots)
    readonly property bool hasClips: shown.some(i => i.kind === "clip")
    property bool installDismissed: false
    readonly property bool installing: FfmpegTool.state !== "idle" && FfmpegTool.state !== "failed"
    property bool active: true
    readonly property bool wanted: active && General.showClips && ((game.platform ?? "") === "steam" || (game.platform ?? "") === "proton" || (game.rom ?? false)) && (game.id ?? "") !== ""

    signal mediaRequested(var item, var list)
    signal menuRequested(var clip, Item tile, point position)
    signal failed(string text)

    visible: wanted && shown.length > 0
    spacing: 10

    function reload() {
        if (wanted)
            Library.media(game.id)
        else
            items = []
    }

    onGameChanged: reload()
    onWantedChanged: reload()
    Component.onCompleted: reload()

    Connections {
        target: Library
        function onMediaReady(id, list) {
            if (id === root.game.id)
                root.items = root.wanted ? list : []
        }
    }

    CollapseHeading {
        id: heading
        text: qsTr("Media")
        section: "clips"
        count: root.shown.length
        onFailed: text => root.failed(text)
    }

    FfmpegPrompt {
        visible: !heading.collapsed && root.hasClips && !FfmpegTool.available && (!root.installDismissed || root.installing)
        Layout.fillWidth: true
        onNotNow: root.installDismissed = true
    }

    // Each fade grows with how far the row can still scroll that way, so an end at rest stays sharp.
    // Invisible, so the layout leaves it out; the scroll bar under the thumbnails stays unfaded.
    Item {
        id: fadeMask
        readonly property real leftFade: Math.min(Theme.media.edgeFade, flick.contentX)
        readonly property real rightFade: Math.min(Theme.media.edgeFade, flick.contentWidth - flick.width - flick.contentX)
        readonly property real fadedHeight: Theme.media.cardPadding + Theme.media.thumbHeight + Theme.media.scrollbarGap
        width: flick.width
        height: flick.height
        visible: false
        layer.enabled: true

        Rectangle {
            width: fadeMask.leftFade
            height: fadeMask.fadedHeight
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: "white" }
            }
        }
        Rectangle {
            x: fadeMask.leftFade
            width: parent.width - fadeMask.leftFade - fadeMask.rightFade
            height: fadeMask.fadedHeight
        }
        Rectangle {
            x: parent.width - width
            width: fadeMask.rightFade
            height: fadeMask.fadedHeight
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "white" }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Rectangle {
            y: fadeMask.fadedHeight
            width: parent.width
            height: parent.height - y
        }
    }

    Surface {
        id: card
        visible: !heading.collapsed
        Layout.fillWidth: true
        Layout.maximumWidth: tiles.width + 2 * Theme.media.cardPadding
        Layout.preferredHeight: Theme.media.cardPadding + Theme.media.thumbHeight
                                + (scrollBar.policy === ScrollBar.AlwaysOn
                                   ? 2 * Theme.media.scrollbarGap + Theme.media.scrollbarHeight : Theme.media.cardPadding)
        radius: Theme.media.cardRadius
        fill: Theme.media.cardBackground
        border.color: Theme.media.cardBorder
        border.width: Theme.media.cardBorderWidth

        Flickable {
            id: flick
            anchors { fill: parent; leftMargin: Theme.media.cardPadding; rightMargin: Theme.media.cardPadding }
            topMargin: Theme.media.cardPadding
            contentWidth: tiles.width
            contentHeight: Theme.media.thumbHeight
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            layer.enabled: Theme.media.edgeFade > 0 && contentWidth > width
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: fadeMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }
            ScrollBar.horizontal: ScrollBar {
                id: scrollBar
                policy: flick.contentWidth > flick.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                padding: 0
                bottomPadding: Theme.media.scrollbarGap
                hoverEnabled: true
                background: Item {
                    Rectangle {
                        anchors { fill: parent; bottomMargin: scrollBar.bottomPadding }
                        radius: height / 2
                        color: Theme.media.scrollbarTrack
                    }
                }
                contentItem: Rectangle {
                    implicitHeight: Theme.media.scrollbarHeight
                    radius: height / 2
                    color: scrollBar.pressed || scrollBar.hovered ? Theme.media.scrollbarHandleHover : Theme.media.scrollbarHandle
                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                }
            }

            Row {
                id: tiles
                spacing: Theme.media.thumbGap

                Repeater {
                    model: root.shown

                    ArtThumb {
                        id: tile
                        required property var modelData
                        readonly property bool hovered: area.containsMouse
                        readonly property bool isClip: modelData.kind === "clip"

                        aspect: 16 / 9
                        cornerRadius: Theme.media.thumbRadius
                        implicitHeight: Theme.media.thumbHeight
                        source: modelData.thumbnail

                    RectangularShadow {
                        z: -1
                        visible: Theme.media.thumbShadowSize > 0
                        anchors.fill: parent
                        offset.y: Theme.media.thumbShadowSize / 6
                        radius: parent.radius
                        blur: Theme.media.thumbShadowSize
                        color: Theme.media.thumbShadowColor
                    }

                        Rectangle {
                            id: scrim
                            readonly property bool fromTop: Theme.media.thumbScrimStyle === "top"
                            anchors.fill: parent
                            radius: parent.radius
                            visible: tile.isClip && Theme.media.thumbScrim.a > 0
                            color: Theme.media.thumbScrimStyle === "full" ? Theme.media.thumbScrim : "transparent"
                            gradient: Theme.media.thumbScrimStyle === "full" ? null : scrimGradient
                            Gradient {
                                id: scrimGradient
                                GradientStop { position: 0; color: scrim.fromTop ? Theme.media.thumbScrim : Qt.rgba(0, 0, 0, 0) }
                                GradientStop { position: 1; color: scrim.fromTop ? Qt.rgba(0, 0, 0, 0) : Theme.media.thumbScrim }
                            }
                        }
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
                            visible: tile.isClip
                            anchors.centerIn: parent
                            width: Theme.media.playSize
                            height: width
                            radius: width / 2
                            color: Theme.media.playBackground

                            Icon {
                                anchors.centerIn: parent
                                path: Icons.playFill
                                size: parent.width / 2
                                color: Theme.media.playIconColor
                            }
                        }
                        Rectangle {
                            visible: tile.isClip
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
                                text: tile.modelData?.length ?? ""
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
                                    root.mediaRequested(tile.modelData, root.shown)
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
}
