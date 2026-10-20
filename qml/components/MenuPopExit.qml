import QtQuick

// Closing is a plain fade of the menu, flattened into one image by SettingsMenu.
Transition {
    MotionNumber { property: "opacity"; to: 0 }
}
