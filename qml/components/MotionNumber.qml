import QtQuick

NumberAnimation {
    property string speed: "fast"
    // For fading large surfaces: the theme's curves start so steeply that a third of the change
    // lands in the first frame, which reads as a flash.
    property bool linear: false

    // A fade keeps its duration under Reduce motion; movement snaps. A Behavior's animation can't
    // read the property it animates, so a Behavior on opacity sets `fade: true`.
    property bool fade: property === "opacity"

    duration: fade ? Motion.fadeDuration(Motion.speedMs(speed)) : Motion.duration(Motion.speedMs(speed))
    easing.type: linear ? Easing.Linear : Motion.easingType
    easing.bezierCurve: linear ? [] : Motion.easingCurve
}
