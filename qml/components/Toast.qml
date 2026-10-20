import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Surface {
    id: root

    property var link: null
    property int minimum: 0
    property bool open: false
    // How far it has slid in from the bottom edge: 0 off the window, 1 shown. `open` is what
    // it should be, this is what you see.
    property real shown: 0
    readonly property int undoDuration: 15000
    readonly property string text: label.text

    function show(text: string, link, minimum = 0) {
        root.minimum = minimum
        label.text = text
        root.link = link ?? null
        open = true
        hideTimer.restart()
    }

    function showUndo(text: string, token) {
        if (token === 0) {
            show(text)
            return
        }
        show(text, {
            text: qsTr("Undo"),
            isUndo: true,
            run: () => {
                const error = Library.undo(token)
                if (error !== "")
                    show(error)
            }
        }, undoDuration)
    }

    function undo() {
        if (open && link?.isUndo === true)
            runLink()
    }

    // Hidden before it runs, so a message the link shows (why it failed) stays up.
    function runLink() {
        const run = root.link.run
        root.open = false
        run()
    }

    implicitWidth: row.implicitWidth + 40
    implicitHeight: 44
    radius: Theme.toast.radius
    fill: Theme.toast.background
    fillEnd: Theme.toast.backgroundEnd
    direction: Theme.toast.gradientDirection
    border.color: Theme.toast.border
    border.width: Theme.toast.borderWidth
    visible: shown > 0

    Behavior on shown {
        id: slide
        enabled: Motion.fades
        MotionNumber { fade: true; speed: slide.targetValue > 0 ? "normal" : "fast" }
    }
    onOpenChanged: shown = open ? 1 : 0
    opacity: Motion.on ? 1 : shown
    transform: Translate {
        y: Motion.on ? (1 - root.shown) * (root.parent ? root.parent.height - root.y : root.height) : 0
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12

        Label {
            id: label
            color: Theme.toast.text
        }

        Label {
            visible: root.link !== null
            text: root.link?.text ?? ""
            color: linkHover.hovered ? Theme.toast.linkHover : Theme.toast.link
            font.underline: linkHover.hovered

            HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onTapped: root.runLink()
            }
        }
    }

    // Stays up while the mouse is on it, so the link can be reached.
    HoverHandler {
        id: hover
        onHoveredChanged: if (!hovered && root.open) hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: Math.max(Theme.toast.duration, root.minimum)
        onTriggered: if (!hover.hovered) root.open = false
    }
}
