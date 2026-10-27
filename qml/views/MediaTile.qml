import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// See docs/code/qml.md.
ArtThumb {
    id: tile

    property var media: ({})
    property real shadowSize: Theme.media.thumbShadowSize
    property color shadowColor: Theme.media.thumbShadowColor
    property color borderColor: Theme.media.thumbBorderColor
    property bool focusRing: false
    readonly property bool hovered: area.containsMouse
    readonly property bool isClip: media.kind === "clip"

    signal clicked()
    signal menuRequested(point position)

    aspect: 16 / 9
    source: media.thumbnail ?? ""

    RectangularShadow {
        z: -1
        visible: tile.shadowSize > 0
        anchors.fill: parent
        offset.y: tile.shadowSize / 6
        radius: parent.radius
        blur: tile.shadowSize
        color: tile.shadowColor
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
        border.color: tile.borderColor
        border.width: Theme.layout.borderWidth
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.filters.focusRingWidth
        radius: parent.radius + Theme.filters.focusRingWidth
        visible: tile.focusRing
        color: "transparent"
        border.color: Theme.filters.focusRing
        border.width: Theme.filters.focusRingWidth
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
            text: tile.media.length ?? ""
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
                tile.menuRequested(Qt.point(mouse.x, mouse.y))
            else
                tile.clicked()
        }
        ToolTip.visible: containsMouse && tile.media.time !== undefined
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: tile.media.time ? tile.media.time.toLocaleDateString(Qt.locale(), General.shortDateFormat) : ""
    }
}
