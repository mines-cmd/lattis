import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    required property string name
    required property string title
    required property var keyLabels
    required property var defaultLabels
    required property bool isCustom
    required property bool conflict
    property bool recording: false
    property bool firstInCard: false
    property bool lastInCard: false
    readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                      Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)

    signal recordRequested()
    signal recordFinished()

    color: hover.hovered ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)

    Behavior on color { enabled: Motion.fades; MotionColor {} }
    radius: Theme.settingsWindow.rowRadius
    // Corners against the card's follow its curve, so the gap around them is even.
    topLeftRadius: firstInCard ? cardCornerRadius : radius
    topRightRadius: topLeftRadius
    bottomLeftRadius: lastInCard ? cardCornerRadius : radius
    bottomRightRadius: bottomLeftRadius
    implicitHeight: Math.max(titles.implicitHeight, controls.height) + Theme.settingsWindow.rowPadding * 2

    HoverHandler { id: hover }

    onRecordingChanged: if (recording) capture.forceActiveFocus()

    // Takes the next key press while recording. Actions.recording turns shortcuts off, so
    // the key reaches here instead of running its action.
    Item {
        id: capture
        Keys.onPressed: event => {
            event.accepted = true
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                root.recordFinished()
                return
            }
            const key = Actions.sequenceFor(event.key, event.modifiers)
            if (key === "")
                return // a modifier on its own; wait for the rest
            Actions.setKeys(root.name, [key])
            root.recordFinished()
        }
        onActiveFocusChanged: if (!activeFocus && root.recording) root.recordFinished()
    }

    Row {
        id: controls
        anchors.right: parent.right
        anchors.rightMargin: Theme.settingsWindow.rowPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.controls.controlSpacing

        Item {
            width: Theme.settingsWindow.editorWidth
            height: keys.height
            anchors.verticalCenter: parent.verticalCenter

            Row {
                id: keys
                anchors.right: parent.right
                spacing: 6

                Repeater {
                    model: root.recording ? [] : root.keyLabels

                    KeyChip {
                        required property string modelData
                        text: modelData
                        conflict: root.conflict
                    }
                }

                KeyChip {
                    visible: root.recording || root.keyLabels.length === 0
                    text: root.recording ? qsTr("Press keys…") : qsTr("Not set")
                    recording: root.recording
                    muted: !root.recording
                }
            }

            MouseArea {
                anchors.fill: keys
                cursorShape: Qt.PointingHandCursor
                onClicked: root.recording ? root.recordFinished() : root.recordRequested()
            }
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            opacity: root.keyLabels.length > 0 && !root.recording ? 1 : 0
            enabled: root.keyLabels.length > 0 && !root.recording

            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            implicitWidth: Theme.settingsWindow.smallButtonSize
            implicitHeight: Theme.settingsWindow.smallButtonSize
            iconSize: Theme.settingsWindow.smallIconSize
            icon: Icons.close
            iconColor: Theme.settingsWindow.resetButtonColor
            tip: qsTr("Remove the shortcut")
            onClicked: Actions.setKeys(root.name, [])
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            opacity: root.isCustom ? 1 : 0
            enabled: root.isCustom

            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            implicitWidth: Theme.settingsWindow.smallButtonSize
            implicitHeight: Theme.settingsWindow.smallButtonSize
            iconSize: Theme.settingsWindow.smallIconSize
            icon: Icons.undo
            iconColor: Theme.settingsWindow.resetButtonColor
            tip: root.defaultLabels.length > 0 ? qsTr("Reset to %1").arg(root.defaultLabels.join(", "))
                                               : qsTr("Reset (no shortcut by default)")
            onClicked: Actions.resetKeys(root.name)
        }
    }

    ColumnLayout {
        id: titles
        anchors.left: parent.left
        anchors.right: controls.left
        anchors.leftMargin: Theme.settingsWindow.rowPadding
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            elide: Text.ElideRight

            HoverHandler { id: titleHover }
            ToolTip.visible: titleHover.hovered
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: "[shortcuts] %1".arg(root.name)
        }
        Unfold {
            id: note
            open: root.conflict || root.recording
            // Out of the layout when folded, so its spacing doesn't add to the row's height.
            visible: open || progress > 0
            Layout.fillWidth: true

            Label {
                id: noteLabel
                // Kept while it folds away, so the note doesn't change to the other one on the way out.
                property bool recordingNote: false
                Connections {
                    target: root
                    function onConflictChanged() { if (note.open) noteLabel.recordingNote = root.recording }
                    function onRecordingChanged() { if (note.open) noteLabel.recordingNote = root.recording }
                }

                Layout.fillWidth: true
                text: noteLabel.recordingNote ? qsTr("Press the new keys; Escape cancels")
                                              : qsTr("Another action uses the same keys")
                color: noteLabel.recordingNote ? Theme.controls.descriptionColor : Theme.settingsWindow.keyChipConflict
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }
        }
    }
}
