import QtQuick

// See docs/code/qml.md.
ParallelAnimation {
    id: root

    property Item target
    // RailButton pops through `popScale`: its `scale` belongs to the press and the drag.
    property string scaleProperty: "scale"
    property string speed: "fast"

    MotionNumber {
        target: root.target; property: root.scaleProperty; to: Motion.on ? Theme.motion.chipScale : 1
        speed: root.speed
    }
    MotionNumber {
        target: root.target; property: "opacity"; to: 0
        speed: root.speed
    }
}
