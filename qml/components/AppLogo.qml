import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Item {
    id: root

    property real size: Theme.rail.logoSize
    property bool highlighted: false

    readonly property bool dog: Theme.rail.logoStyle === "dog"
    readonly property real radius: Theme.rail.logoRadius * size / Theme.rail.logoSize

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        visible: !root.dog
        radius: root.radius
        color: Theme.rail.logoBackground
        border.color: Theme.rail.logoBorder
        border.width: Theme.rail.logoBorderWidth

        Label {
            anchors.centerIn: parent
            text: Theme.rail.logoText
            color: Theme.rail.logoTextColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: parent.height * 0.38
        }
    }

    Loader {
        anchors.fill: parent
        active: root.dog
        sourceComponent: Component { DogLogo { radius: root.radius } }
    }

    property real lift: highlighted ? 1 : 0
    Behavior on lift { enabled: Motion.on; MotionNumber {} }

    Rectangle {
        anchors.fill: parent
        visible: root.lift > 0
        opacity: root.lift
        radius: root.radius
        color: Theme.rail.logoHover
    }
}
