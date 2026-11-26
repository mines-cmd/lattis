import QtQuick
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property real size: Theme.rail.logoSize
    property bool highlighted: false
    property bool updateWaiting: false

    readonly property real radius: Theme.rail.logoRadius * size / Theme.rail.logoSize

    readonly property real dotSize: Theme.rail.updateDotSize
    readonly property real dotX: Theme.rail.updateDotCorner.endsWith("left")
                                 ? Theme.rail.updateDotMargin : width - dotSize - Theme.rail.updateDotMargin
    readonly property real dotY: Theme.rail.updateDotCorner.startsWith("top")
                                 ? Theme.rail.updateDotMargin : height - dotSize - Theme.rail.updateDotMargin
    readonly property bool cut: updateWaiting && Theme.rail.updateDotGap > 0
    property real lift: highlighted ? 1 : 0
    Behavior on lift { enabled: Motion.on; MotionNumber {} }

    implicitWidth: size
    implicitHeight: size

    // The margin keeps the tile's smooth edge inside the layer.
    Item {
        anchors.fill: parent
        anchors.margins: -2
        layer.enabled: root.cut
        layer.effect: MultiEffect {
            maskEnabled: true
            maskInverted: true
            maskSource: hole
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
        }

        Item {
            anchors.fill: parent
            anchors.margins: 2

            DogLogo {
                anchors.fill: parent
                radius: root.radius
            }

            Rectangle {
                anchors.fill: parent
                visible: root.lift > 0
                opacity: root.lift
                radius: root.radius
                color: Theme.rail.logoHover
            }
        }
    }

    Item {
        id: hole
        anchors.fill: parent
        anchors.margins: -2
        visible: false
        layer.enabled: root.cut

        Rectangle {
            x: root.dotX + 2 - Theme.rail.updateDotGap
            y: root.dotY + 2 - Theme.rail.updateDotGap
            width: root.dotSize + 2 * Theme.rail.updateDotGap
            height: width
            radius: width / 2
        }
    }

    Rectangle {
        visible: root.updateWaiting
        x: root.dotX
        y: root.dotY
        width: root.dotSize
        height: width
        radius: width / 2
        color: Theme.rail.updateDot
    }
}
