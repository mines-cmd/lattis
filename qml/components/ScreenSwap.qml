import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property bool shown: false
    property int side: 1

    readonly property int slide: Motion.duration(Theme.motion.normal)
    property real shift: 0
    property bool ready: false

    anchors.fill: parent
    opacity: shown ? 1 : 0
    visible: opacity > 0
    enabled: shown
    transform: Translate { y: root.shift }

    Component.onCompleted: Qt.callLater(() => { ready = true })

    onShownChanged: {
        if (!ready) {
            opacity = shown ? 1 : 0
            return
        }
        slideOut.stop()
        slideIn.stop()
        if (shown) {
            opacity = 0
            shift = side * Theme.settingsWindow.pageOffset
            slideIn.start()
        } else {
            slideOut.start()
        }
    }

    ParallelAnimation {
        id: slideOut
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: Math.round(root.slide / 2) }
        NumberAnimation {
            target: root
            property: "shift"
            to: root.side * Theme.settingsWindow.pageOffset
            duration: root.slide
            easing.type: Motion.easingType
            easing.bezierCurve: Motion.easingCurve
        }
    }
    ParallelAnimation {
        id: slideIn
        NumberAnimation { target: root; property: "opacity"; to: 1; duration: Math.round(root.slide / 2) }
        NumberAnimation {
            target: root
            property: "shift"
            to: 0
            duration: root.slide
            easing.type: Motion.easingType
            easing.bezierCurve: Motion.easingCurve
        }
    }
}
