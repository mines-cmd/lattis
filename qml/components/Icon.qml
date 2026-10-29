import QtQuick
import QtQuick.Shapes

// See docs/code/qml.md.
Item {
    id: root

    property string path
    property color color: Theme.text
    property real size: 20
    property int fillRule: ShapePath.WindingFill

    property real strokeWidth: Theme.layout.iconStroke
    readonly property bool stroked: path.startsWith("stroke:")

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.stroked ? "transparent" : root.color
            fillRule: root.fillRule
            strokeColor: root.stroked ? root.color : "transparent"
            strokeWidth: root.stroked ? root.strokeWidth : 0
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.stroked ? root.path.slice(7) : root.path }
        }
    }
}
