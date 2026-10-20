import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    property Item box: parent
    property var value

    property bool ready: false
    Component.onCompleted: Qt.callLater(() => { ready = true })

    onValueChanged: if (ready && Motion.on) bump.restart()

    SequentialAnimation {
        id: bump
        MotionNumber { target: root.box; property: "scale"; to: 1 + (1 - Theme.motion.pressScale) * 3 }
        MotionPop { target: root.box; property: "scale"; to: 1 }
    }
}
