import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Rectangle {
    id: root

    property string checkedButton: ""

    signal triggered(string name)

    readonly property var known: ({
        stats: { icon: Icons.barChartLine, tip: qsTr("Play statistics"), action: "play_statistics" }
    })
    readonly property var buttons: Theme.toolbar.buttons.filter(b => known[b] !== undefined)

    visible: buttons.length > 0
    implicitWidth: row.implicitWidth + 2 * Theme.toolbar.padding
    implicitHeight: row.implicitHeight + 2 * Theme.toolbar.padding
    radius: Theme.toolbar.backgroundRadius
    color: Theme.toolbar.background
    border.color: Theme.toolbar.border
    border.width: Theme.layout.borderWidth

    // Keeps the strip's drag-to-move zone from starting under the toolbar.
    MouseArea { anchors.fill: parent }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.toolbar.spacing

        Repeater {
            model: root.buttons

            Surface {
                id: button
                required property string modelData
                readonly property var info: root.known[modelData]
                readonly property bool checked: root.checkedButton === modelData

                width: Theme.toolbar.size
                height: Theme.toolbar.size
                radius: Theme.toolbar.radius
                readonly property color restFill: mouse.containsMouse ? Theme.toolbar.hoverBackground
                                                                      : Qt.alpha(Theme.toolbar.hoverBackground, 0)
                fill: checked ? Theme.toolbar.checkedBackground : restFill
                fillEnd: checked ? Theme.toolbar.checkedBackgroundEnd : restFill
                direction: Theme.layout.gradientDirection
                scale: mouse.pressed ? Theme.motion.pressScale : 1

                Behavior on scale { enabled: Motion.on; MotionNumber {} }
                Behavior on fill { enabled: Motion.fades; MotionColor {} }
                Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

                Icon {
                    anchors.centerIn: parent
                    size: Theme.toolbar.iconSize
                    color: button.checked ? Theme.toolbar.checkedIcon : Theme.toolbar.icon
                    path: button.info.icon

                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.triggered(button.info.action)
                }

                ToolTip.visible: mouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: Actions.tip(button.info.tip, Actions.keys[button.info.action] ?? [])
            }
        }
    }
}
