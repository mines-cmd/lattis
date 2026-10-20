import QtQuick

ColorAnimation {
    property string speed: "fast"

    duration: Motion.fadeDuration(Motion.speedMs(speed))
    easing.type: Motion.easingType
    easing.bezierCurve: Motion.easingCurve
}
