import QtQuick

// A menu opening beside what opened it: it springs out sideways from motion.menu_offset to its left
// while it grows from motion.menu_scale and fades in. The menu declares `property real slide: 0` and
// translates its background and list by it, since popup() owns its x.
Transition {
    MotionNumber { property: "opacity"; from: 0; to: 1; speed: "fast" }
    MotionPop { property: "scale"; from: Theme.motion.menuScale; to: 1 }
    MotionPop { property: "slide"; from: -Theme.motion.menuOffset; to: 0 }
}
