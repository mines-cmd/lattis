import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property url source
    property string title
    property real hue: 0
    property real size: Theme.list.iconSize

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        visible: icon.status !== Image.Ready || icon.opacity < 1
        radius: Theme.list.iconRadius
        color: Qt.hsla(root.hue, Theme.list.iconPlaceholderSaturation, Theme.list.iconPlaceholderLightness, 1)

        Label {
            anchors.centerIn: parent
            text: root.title.charAt(0)
            color: Theme.list.iconPlaceholderText
            font.pixelSize: root.size * 0.6
            font.bold: true
        }
    }

    // Fitted to the painted picture, so an icon that isn't square still gets its corners rounded.
    Rectangle {
        id: iconMask
        width: icon.width
        height: icon.height
        visible: false
        layer.enabled: true
        color: "transparent"

        Rectangle {
            anchors.centerIn: parent
            width: icon.paintedWidth
            height: icon.paintedHeight
            radius: Theme.list.iconRadius
        }
    }

    FadeImage {
        id: icon
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true

        // Masked only once loaded: a layer made before the picture arrived could stay blank until redrawn.
        layer.enabled: status === Image.Ready && Theme.list.iconRadius > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: iconMask
            // Without a threshold every half-covered edge pixel counts as inside, which eats the corner's curve.
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }
}
