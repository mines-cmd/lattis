import QtQuick

// See docs/code/qml.md.
Rectangle {
    id: root

    property color fill
    property color fillEnd: fill
    property string direction: "down"

    color: fill
    // A square, unantialiased rectangle that switches between gradient and flat can stay in an
    // opaque batch and write alpha 0 for a transparent fill, punching a hole to the desktop.
    antialiasing: true
    gradient: Qt.colorEqual(fill, fillEnd) ? null : grad

    Gradient {
        id: grad
        orientation: root.direction === "right" || root.direction === "left"
                     ? Gradient.Horizontal : Gradient.Vertical
        GradientStop {
            position: 0
            color: root.direction === "up" || root.direction === "left" ? root.fillEnd : root.fill
        }
        GradientStop {
            position: 1
            color: root.direction === "up" || root.direction === "left" ? root.fill : root.fillEnd
        }
    }
}
