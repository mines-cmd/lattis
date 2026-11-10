import QtQuick
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property int radius: Theme.popover.radius
    property color fill: Theme.popover.background
    property color fillEnd: fill
    property string direction: "down"
    property color borderColor: Theme.popover.border
    property int borderWidth: Theme.popover.borderWidth
    property int shadowSize: Theme.popover.shadowSize
    property color shadowColor: Theme.popover.shadowColor

    RectangularShadow {
        visible: root.shadowSize > 0
        anchors.fill: parent
        offset.y: root.shadowSize / 6
        radius: root.radius
        blur: root.shadowSize
        color: root.shadowColor
    }
    Surface {
        anchors.fill: parent
        radius: root.radius
        fill: root.fill
        fillEnd: root.fillEnd
        direction: root.direction
        border.color: root.borderColor
        border.width: root.borderWidth
    }
}
