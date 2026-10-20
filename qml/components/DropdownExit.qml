import QtQuick

Transition {
    SequentialAnimation {
        ParallelAnimation {
            // Under Reduce motion it stays open while the entries fade, rather than vanishing at once.
            MotionNumber { property: "reveal"; to: Motion.on ? 0 : 1; speed: "normal" }
            MotionNumber { property: "opacity"; to: 0; speed: "fast" }
        }
        // Left at 0, the next popup() would place a menu as if it had no height and not flip it
        // above the cursor near the window's bottom.
        PropertyAction { property: "reveal"; value: 1 }
    }
}
