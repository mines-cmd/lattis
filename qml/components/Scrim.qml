import QtQuick

// A modal popup's Overlay.modal: the popup sets its opacity to 1 on open and 0 on close, so it
// fades with the popup instead of appearing at once.
Rectangle {
    color: Theme.gameEditor.scrim

    Behavior on opacity {
        id: fade
        enabled: Motion.fades
        MotionNumber { fade: true; speed: fade.targetValue > 0 ? "normal" : "fast"; linear: true }
    }
}
