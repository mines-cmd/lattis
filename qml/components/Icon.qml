import QtQuick
import QtQuick.Shapes

// See docs/code/qml.md.
Item {
    id: root

    property string path
    property color color: Theme.text
    property real size: 20
    property int fillRule: ShapePath.WindingFill

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.color
            fillRule: root.fillRule
            strokeColor: "transparent"
            strokeWidth: 0

            PathSvg { path: root.path }
        }
    }
}
