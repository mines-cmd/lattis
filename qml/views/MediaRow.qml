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
    signal showAllRequested(string id)

    visible: wanted && shown.length > 0
    spacing: 10 * reveal

    property real reveal: heading.collapsed ? 0 : 1
    Behavior on reveal { enabled: Motion.on; MotionNumber { speed: "fast" } }

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

    Item {
        id: headRow
        Layout.fillWidth: true
        Layout.maximumWidth: tiles.width + 2 * Theme.media.cardPadding
        implicitHeight: heading.implicitHeight

        CollapseHeading {
            id: heading
            width: implicitWidth
            height: parent.height
            text: qsTr("Media")
            section: "clips"
            count: root.shown.length
            onFailed: text => root.failed(text)
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            readonly property real collapsedX: heading.width + Theme.layout.spacing
            x: collapsedX + (headRow.width - width - Theme.media.linkInset - collapsedX) * root.reveal
            text: qsTr("Show all")
            color: showAll.containsMouse ? Theme.media.linkHoverColor : Theme.media.linkColor
            font.pixelSize: Theme.details.countFontSize
            Behavior on color { enabled: Motion.fades; MotionColor {} }

            MouseArea {
                id: showAll
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.showAllRequested(root.game.id)
            }
        }
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
        visible: root.reveal > 0
        clip: root.reveal < 1
        opacity: root.reveal
        Layout.fillWidth: true
        Layout.maximumWidth: (tiles.width + 2 * Theme.media.cardPadding) * root.reveal
        Layout.preferredHeight: (Theme.media.cardPadding + Theme.media.thumbHeight
                                 + (scrollBar.policy === ScrollBar.AlwaysOn
                                    ? 2 * Theme.media.scrollbarGap + Theme.media.scrollbarHeight : Theme.media.cardPadding))
                                * root.reveal
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
                policy: flick.contentWidth > headRow.width - 2 * Theme.media.cardPadding ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
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

                    MediaTile {
                        id: tile
                        required property var modelData

                        media: modelData
                        cornerRadius: Theme.media.thumbRadius
                        implicitHeight: Theme.media.thumbHeight
                        onClicked: root.mediaRequested(modelData, root.shown)
                        onMenuRequested: position => root.menuRequested(modelData, tile, position)
                    }
                }
            }
        }
    }
}
