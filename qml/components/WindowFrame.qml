import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    readonly property var window: Window.window
    readonly property int border: Theme.windowControls.resizeBorder
    readonly property int corner: border * 2

    anchors.fill: parent
    visible: border > 0 && window !== null && window.visibility === Window.Windowed

    component Edge: MouseArea {
        required property int edges
        cursorShape: {
            switch (edges) {
            case Qt.LeftEdge:
            case Qt.RightEdge: return Qt.SizeHorCursor
            case Qt.TopEdge:
            case Qt.BottomEdge: return Qt.SizeVerCursor
            case Qt.TopEdge | Qt.LeftEdge:
            case Qt.BottomEdge | Qt.RightEdge: return Qt.SizeFDiagCursor
            default: return Qt.SizeBDiagCursor
            }
        }
        onPressed: root.window.startSystemResize(edges)
    }

    Edge { edges: Qt.LeftEdge; x: 0; y: root.corner; width: root.border; height: root.height - 2 * root.corner }
    Edge { edges: Qt.RightEdge; x: root.width - root.border; y: root.corner; width: root.border; height: root.height - 2 * root.corner }
    Edge { edges: Qt.TopEdge; x: root.corner; y: 0; width: root.width - 2 * root.corner; height: root.border }
    Edge { edges: Qt.BottomEdge; x: root.corner; y: root.height - root.border; width: root.width - 2 * root.corner; height: root.border }
    Edge { edges: Qt.TopEdge | Qt.LeftEdge; x: 0; y: 0; width: root.corner; height: root.corner }
    Edge { edges: Qt.TopEdge | Qt.RightEdge; x: root.width - root.corner; y: 0; width: root.corner; height: root.corner }
    Edge { edges: Qt.BottomEdge | Qt.LeftEdge; x: 0; y: root.height - root.corner; width: root.corner; height: root.corner }
    Edge { edges: Qt.BottomEdge | Qt.RightEdge; x: root.width - root.corner; y: root.height - root.corner; width: root.corner; height: root.corner }
}
