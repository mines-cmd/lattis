import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Rectangle {
    id: root

    property string name
    property string detail
    property int nameWeight: Theme.tags.fontWeight === "thin" ? Font.Thin
                           : Theme.tags.fontWeight === "semibold" ? Font.DemiBold
                           : Theme.tags.fontWeight === "bold" ? Font.Bold : Font.Normal
    property bool spread: false
    property bool plus: true
    readonly property alias truncated: nameLabel.truncated
    property color textColor: fillText
    property color detailColor: textColor
    property int colorIndex: 0
    property bool outlined: false
    property int outlineWidth: Theme.tags.outlineWidth
    property real fillOpacity: 1
    property bool dot: false
    property color dotColor: fillText
    property real dotSize: 0
    property bool removable: false
    property string removeTip
    property bool lit: false
    property bool raised: false
    property real raiseBy: 0
    property bool pressed: false
    property real lift: raised ? raiseBy : 0
    property real press: pressed ? Theme.motion.pressScale : 1
    readonly property bool removeHovered: removeMouse.containsMouse

    signal removed()

    readonly property var tagColors: Theme.tags.palette
    readonly property color tagColor: tagColors.length > 0 ? tagColors[colorIndex % tagColors.length]
                                                           : Theme.palette.surfaceRaised
    readonly property color fillText: Theme.tags.textStyle === "custom" ? Theme.tags.text
        : Qt.hsla(tagColor.hslHue, tagColor.hslSaturation,
                  Theme.tags.textStyle === "light" ? Theme.tags.lightTextLightness : Theme.tags.darkTextLightness, 1)

    // With the ×, the right padding shrinks so the × sits about as far from the edge as from the top.
    implicitWidth: content.implicitWidth + Theme.tags.padding
                   + (removable ? (Theme.tags.height - remove.width) / 2 : Theme.tags.padding)
    implicitHeight: Theme.tags.height
    radius: Theme.tags.radius
    color: outlined ? Qt.rgba(tagColor.r, tagColor.g, tagColor.b, fillOpacity) : tagColor
    border.color: outlined ? tagColor : Theme.tags.border
    border.width: outlined ? outlineWidth : Theme.tags.borderWidth

    Behavior on lift { enabled: Motion.on; MotionNumber {} }
    Behavior on press { enabled: Motion.on; MotionNumber {} }
    // Transforms, not y and scale: a Flow places the chips and its add and leave animations scale them.
    transform: [
        Translate { y: -root.lift },
        Scale {
            origin.x: root.width / 2
            origin.y: root.height / 2
            xScale: root.press
            yScale: root.press
        }
    ]

    // A layer of its own, so only the hover fades: the chip's own color also changes for a
    // recolor or a morph, which mustn't animate.
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Qt.lighter(root.tagColor, Theme.tags.hoverLighten)
        visible: !root.outlined
        opacity: root.lit ? 1 : 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    }

    Row {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        x: root.removable || root.spread ? Theme.tags.padding : (root.width - width) / 2
        spacing: Theme.tags.fontSize / 4

        Icon {
            visible: root.outlined && root.plus
            anchors.verticalCenter: parent.verticalCenter
            path: Icons.add
            size: Theme.tags.fontSize
            color: root.textColor
        }
        Rectangle {
            visible: root.dot
            anchors.verticalCenter: parent.verticalCenter
            width: root.dotSize
            height: root.dotSize
            radius: root.dotSize / 2
            color: root.dotColor
        }
        Label {
            id: nameLabel
            anchors.verticalCenter: parent.verticalCenter
            width: root.spread ? Math.max(0, root.width - 2 * Theme.tags.padding - (detailLabel.visible ? detailLabel.implicitWidth + content.spacing : 0))
                               : implicitWidth
            elide: Text.ElideRight
            text: root.name
            color: root.textColor
            font.pixelSize: Theme.tags.fontSize
            font.weight: root.nameWeight
        }
        Label {
            id: detailLabel
            visible: root.detail !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: root.detail
            color: root.detailColor
            font.pixelSize: Theme.tags.fontSize
            font.weight: Font.DemiBold
        }
        Rectangle {
            id: remove
            visible: root.removable
            anchors.verticalCenter: parent.verticalCenter
            // Whole pixels with the same parity as the ×, or centering it snaps it half a pixel off.
            width: Math.round(Theme.tags.height * 0.6) + (Math.round(Theme.tags.height * 0.6) - removeIcon.size) % 2
            height: width
            radius: width / 2
            color: removeMouse.containsMouse ? Theme.tags.removeHover : Qt.alpha(Theme.tags.removeHover, 0)

            Behavior on color { enabled: Motion.fades; MotionColor {} }

            Icon {
                id: removeIcon
                anchors.centerIn: parent
                path: Icons.close
                size: Math.round(Theme.tags.height / 2.5)
                color: root.fillText
            }
        }
    }

    // Raised above whatever MouseArea the chip's user fills it with.
    MouseArea {
        id: removeMouse
        z: 1
        visible: root.removable
        x: content.x + remove.x
        y: content.y + remove.y
        width: remove.width
        height: remove.height
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.removed()

        ToolTip.visible: root.removeTip !== "" && containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: root.removeTip
    }
}
