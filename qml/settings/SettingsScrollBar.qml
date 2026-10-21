import QtQuick
import QtQuick.Controls.Basic

ScrollBar {
    id: root

    property int thickness: Theme.controls.scrollbarWidth
    property bool trackless: false
    property color handleColor: Theme.controls.scrollbarHandle
    property color handleHoverColor: Theme.controls.scrollbarHandleHover

    policy: ScrollBar.AlwaysOn
    visible: size < 1
    padding: 0
    minimumSize: 0.05
    hoverEnabled: true

    background: Rectangle {
        implicitWidth: root.thickness
        radius: width / 2
        color: root.trackless ? "transparent" : Theme.controls.scrollbarTrack
    }

    function glide(fromY, duration) {
        handle.offset = fromY - contentItem.y
        handleGlide.duration = duration
        handleGlide.restart()
    }

    // The control places contentItem from C++, which a Behavior can't intercept, so the handle
    // is drawn inside it with an offset that can be animated.
    contentItem: Item {
        implicitWidth: root.thickness

        Rectangle {
            id: handle
            property real offset: 0
            y: offset
            width: parent.width
            height: parent.height
            radius: width / 2
            color: root.pressed || root.hovered ? root.handleHoverColor
                 : root.handleColor

            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
    }

    NumberAnimation {
        id: handleGlide
        target: handle
        property: "offset"
        to: 0
        easing.type: Motion.easingType
        easing.bezierCurve: Motion.easingCurve
    }
}
