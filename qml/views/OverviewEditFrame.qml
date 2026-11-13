import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Item {
    id: root

    property bool editing: false
    property string title
    property bool hiddenCard: false
    property bool canDrag: true

    // Pointer positions are in scene coordinates: the card moves under the pointer while it is dragged.
    signal eyeClicked()
    signal dragStarted(point scene)
    signal dragMoved(point scene)
    signal dragEnded(bool dropped)

    visible: editing
    z: 5

    Loader {
        anchors.fill: parent
        active: root.editing

        sourceComponent: Item {
            // The card takes no clicks while it is edited.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
            }

            Rectangle {
                // The driver finds a control by its text.
                property string text: qsTr("Move %1").arg(root.title)

                visible: root.canDrag
                x: Theme.overview.editButtonInset
                y: -height / 2
                width: Theme.overview.editButtonSize
                height: width
                radius: Theme.overview.editButtonRadius
                color: Theme.overview.editButtonBackground
                border.color: Theme.overview.editButtonBorder
                border.width: Theme.stats.cardBorderWidth

                DragGrip {
                    anchors.fill: parent
                    color: Theme.overview.editIcon
                    hoverColor: Theme.overview.editIconHover
                    iconSize: Theme.overview.editIconSize
                    onDragStarted: (handle, x, y) => root.dragStarted(handle.mapToItem(null, x, y))
                    onDragMoved: (handle, x, y) => root.dragMoved(handle.mapToItem(null, x, y))
                    onDragEnded: dropped => root.dragEnded(dropped)
                }
            }

            Rectangle {
                id: eye

                // The driver finds a control by its text.
                property string text: root.hiddenCard ? qsTr("Show %1").arg(root.title) : qsTr("Hide %1").arg(root.title)

                x: parent.width - width - Theme.overview.editButtonInset
                y: -height / 2
                width: Theme.overview.editButtonSize
                height: width
                radius: Theme.overview.editButtonRadius
                color: Theme.overview.editButtonBackground
                border.color: Theme.overview.editButtonBorder
                border.width: Theme.stats.cardBorderWidth
                scale: eyeMouse.pressed ? Theme.motion.pressScale : 1

                Behavior on scale { enabled: Motion.on; MotionNumber {} }

                Icon {
                    anchors.centerIn: parent
                    size: Theme.overview.editIconSize
                    path: root.hiddenCard ? Icons.hidden : Icons.eyeLine
                    color: eyeMouse.containsMouse ? Theme.overview.editIconHover : Theme.overview.editIcon
                }
                MouseArea {
                    id: eyeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.eyeClicked()
                }
                Accessible.role: Accessible.Button
                Accessible.name: eye.text
                ToolTip.visible: eyeMouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: eye.text
            }
        }
    }
}
