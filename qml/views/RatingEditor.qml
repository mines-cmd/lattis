import QtQuick
import QtQuick.Controls.Basic

FocusScope {
    id: root

    property real value: -1
    readonly property real hoverValue: mouse.containsMouse ? valueAt(mouse.mouseX) : -1
    readonly property real target: hoverValue >= 0 ? hoverValue : Math.max(0, value)
    // Springs to the hovered rating, and back to the rating when the pointer leaves. Set here rather
    // than bound, so a rating loaded with a game (not hovered or focused) can skip the spring: a
    // binding toggling the Behavior raced the hover change that moved the target.
    property real shown: 0
    property real seenValue: -1
    property bool snap: true

    Behavior on shown {
        enabled: Motion.on && !root.snap
        MotionSpring { epsilon: 0.001 }
    }
    onTargetChanged: {
        const loaded = value !== seenValue && !mouse.containsMouse && !activeFocus
        seenValue = value
        const wasSnap = snap
        snap = snap || loaded
        shown = target
        snap = wasSnap
    }
    Component.onCompleted: {
        seenValue = value
        shown = target
        snap = false
    }

    implicitWidth: stars.implicitWidth
    implicitHeight: stars.implicitHeight
    activeFocusOnTab: true

    function valueAt(x: real): real {
        // The gap after a star belongs to its right half, so no point between the stars is dead.
        const step = Theme.gameEditor.starSize + stars.spacing
        const clamped = Math.max(0, Math.min(x, width - 1))
        const half = Math.floor(clamped / step) + ((clamped % step) < Theme.gameEditor.starSize / 2 ? 0.5 : 1)
        return Math.min(5, half)
    }

    Keys.onLeftPressed: value = value <= 0.5 ? -1 : value - 0.5
    Keys.onRightPressed: value = Math.min(5, Math.max(0, value) + 0.5)

    Stars {
        id: stars
        spacing: Theme.gameEditor.starSpacing
        starSize: Theme.gameEditor.starSize
        value: Math.min(root.value, root.shown)
        preview: root.shown
        previewOpacity: Theme.gameEditor.starPreviewOpacity
        color: Theme.gameEditor.star
        emptyColor: Theme.gameEditor.starEmpty
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -2 * Theme.gameEditor.starSpacing - Theme.settingsWindow.inputBorderWidth
        visible: root.activeFocus
        color: "transparent"
        radius: Theme.settingsWindow.inputRadius
        border.color: Theme.settingsWindow.inputBorderFocus
        border.width: Theme.settingsWindow.inputBorderWidth
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: event => {
            root.value = root.valueAt(event.x)
            root.forceActiveFocus()
        }
        onDoubleClicked: root.value = -1
    }
}
