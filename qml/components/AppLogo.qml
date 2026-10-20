import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Window

// See docs/code/qml.md.
Item {
    id: root

    property real size: Theme.rail.logoSize
    property bool highlighted: false

    readonly property bool image: Theme.rail.logoStyle === "image"
    // The simple mark (no halo, bevel or shadow), so the squircle fills the logo's square, as
    // the app icon's doesn't (it's inset for its halo).
    readonly property url simpleLogo: "qrc:/qt/qml/Launcher/resources/logo/lattis-ember-simple.svg"
    readonly property int pixels: Math.ceil(size * Screen.devicePixelRatio)
    readonly property real radius: Theme.rail.logoRadius * size / Theme.rail.logoSize

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: square
        anchors.fill: parent
        visible: !root.image
        radius: root.radius
        color: Theme.rail.logoBackground
        border.color: Theme.rail.logoBorder
        border.width: Theme.rail.logoBorderWidth

        // The path fills 22 of Icon's 24 units, so scale the icon up to make the mark itself
        // logo_mark_size percent of the logo.
        Icon {
            visible: Theme.rail.logoStyle === "mark"
            anchors.centerIn: parent
            path: Icons.appLogo
            color: Theme.rail.logoMark
            size: parent.height * Theme.rail.logoMarkSize / 100 * 24 / 22
        }

        Label {
            visible: Theme.rail.logoStyle === "text"
            anchors.centerIn: parent
            text: Theme.rail.logoText
            color: Theme.rail.logoTextColor
            font.bold: true
            font.pixelSize: parent.height * 0.38
        }
    }

    Image {
        id: icon
        anchors.fill: parent
        visible: root.image
        source: root.image ? root.simpleLogo : ""
        sourceSize: Qt.size(root.pixels, root.pixels)
        smooth: true
    }

    property real lift: highlighted ? 1 : 0
    Behavior on lift { enabled: Motion.on; MotionNumber {} }

    Rectangle {
        anchors.fill: parent
        visible: root.lift > 0 && !root.image
        opacity: root.lift
        radius: root.radius
        color: Theme.rail.logoHover
    }
    Rectangle {
        id: tint
        anchors.fill: parent
        visible: false
        color: Theme.rail.logoHover
        layer.enabled: root.lift > 0 && root.image
    }
    Image {
        id: shape
        anchors.fill: parent
        visible: false
        layer.enabled: true
        source: root.lift > 0 && root.image ? root.simpleLogo : ""
        sourceSize: Qt.size(root.pixels, root.pixels)
        smooth: true
    }
    MultiEffect {
        anchors.fill: parent
        visible: root.lift > 0 && root.image
        opacity: root.lift
        source: tint
        maskEnabled: true
        maskSource: shape
    }
}
