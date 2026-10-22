import QtQuick

// See docs/code/qml.md.
ParallelAnimation {
    id: root

    property Item target
    // RailButton pops through `popScale`: its `scale` belongs to the press and the drag.
    property string scaleProperty: "scale"
    property string speed: "fast"
    // The entry's `leaving`. It can turn false again before the model removes the entry (brought back
    // within its leave time); the target then comes back, or it would stay in the layout unseen.
    property bool leaving: false
    property bool animated: Motion.on

    onLeavingChanged: {
        const going = leaving ? root : back
        const other = leaving ? back : root
        other.stop()
        going.start()
        // Through the animation even when snapping, so a binding on the target's opacity survives.
        if (!animated)
            going.complete()
    }

    MotionNumber {
        target: root.target; property: root.scaleProperty; to: Motion.on ? Theme.motion.chipScale : 1
        speed: root.speed
    }
    MotionNumber {
        target: root.target; property: "opacity"; to: 0
        speed: root.speed
    }

    readonly property ParallelAnimation back: ParallelAnimation {
        MotionNumber { target: root.target; property: "opacity"; to: 1; speed: root.speed }
        MotionPop { target: root.target; property: root.scaleProperty; to: 1; speed: "normal" }
    }
}
