import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    property Item box: parent
    property var value

    property bool ready: false
    Timer { running: true; interval: 0; onTriggered: root.ready = true }

    onValueChanged: if (ready && Motion.on) bump.restart()

    SequentialAnimation {
        id: bump
        MotionNumber { target: root.box; property: "scale"; to: 1 + (1 - Theme.motion.pressScale) * 3 }
        MotionPop { target: root.box; property: "scale"; to: 1 }
    }
}
