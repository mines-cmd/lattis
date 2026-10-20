import QtQuick

// A dropdown or menu opening: it drops down from its top edge to its full height while it fades in.
// The popup declares `property real reveal: 1` and `height: implicitHeight * reveal`, and clips,
// or its content shows below the growing edge. Animating height itself would freeze the target at
// the implicitHeight of the moment it opens, before a menu has laid out its entries.
Transition {
    MotionNumber { property: "reveal"; from: 0; to: 1; speed: "normal" }
    MotionNumber { property: "opacity"; from: 0; to: 1; speed: "fast" }
}
