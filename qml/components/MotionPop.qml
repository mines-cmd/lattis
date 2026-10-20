import QtQuick

NumberAnimation {
    property string speed: "normal"

    duration: Motion.duration(Motion.speedMs(speed))
    easing.type: Easing.OutBack
    easing.overshoot: Theme.motion.overshoot
}
