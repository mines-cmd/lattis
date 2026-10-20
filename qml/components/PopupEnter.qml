import QtQuick

Transition {
    MotionNumber { property: "opacity"; from: 0; to: 1; speed: "normal" }
    MotionPop { property: "scale"; from: Theme.motion.popupScale; to: 1 }
}
