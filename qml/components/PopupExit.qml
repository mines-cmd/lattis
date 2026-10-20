import QtQuick

Transition {
    MotionNumber { property: "opacity"; to: 0; speed: "fast" }
    MotionNumber { property: "scale"; to: Motion.on ? Theme.motion.popupScale : 1; speed: "fast" }
}
