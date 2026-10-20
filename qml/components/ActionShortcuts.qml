import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    required property string scope
    property var only: null

    Repeater {
        model: Actions.names(root.scope)

        Item {
            id: holder
            required property string modelData

            Shortcut {
                sequences: Actions.keys[holder.modelData]
                // Only in this window, not while a child window of it has focus (see Actions).
                enabled: !Actions.recording && Actions.focusWindow === holder.Window.window
                         && (root.only === null || root.only.includes(holder.modelData))
                onActivated: Actions.trigger(holder.modelData)
            }
        }
    }
}
