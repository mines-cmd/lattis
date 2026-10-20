import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    property bool shown: false
    property string speed: "fast"
    property string popSpeed: "normal"
    property real hiddenScale: Theme.motion.markScale
    property real overshoot: Theme.motion.markOvershoot

    // States, not Behaviors: the way in overshoots and the way out doesn't, and a Behavior can't
    // tell which way the value is going.
    property bool ready: false
    Component.onCompleted: Qt.callLater(() => { ready = true })

    visible: opacity > 0

    states: State {
        name: "hidden"
        when: !root.shown
        PropertyChanges { root.opacity: 0; root.scale: Motion.on ? root.hiddenScale : 1 }
    }
    transitions: [
        Transition {
            to: "hidden"
            enabled: Motion.fades && root.ready
            MotionNumber { property: "opacity"; speed: root.speed }
            MotionNumber { property: "scale"; speed: root.speed }
        },
        Transition {
            from: "hidden"
            enabled: Motion.fades && root.ready
            MotionNumber { property: "opacity"; speed: root.speed }
            MotionPop { property: "scale"; speed: root.popSpeed; easing.overshoot: root.overshoot }
        }
    ]
}
