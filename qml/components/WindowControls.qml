import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Rectangle {
    id: root

    property bool panel: false
    property var dialog: null

    readonly property var window: Window.window
    readonly property bool maximized: window !== null && window.visibility === Window.Maximized
    readonly property var buttons: Theme.windowControls.buttons.filter(
        b => dialog !== null ? b === "close" : (b !== "minimize" || WindowState.canMinimize))

    visible: buttons.length > 0 && window !== null && window.visibility !== Window.FullScreen
    implicitWidth: row.implicitWidth + 2 * padding
    implicitHeight: row.implicitHeight + 2 * padding
    readonly property int padding: panel ? Theme.windowControls.padding : 0
    radius: Theme.windowControls.backgroundRadius
    color: panel ? Theme.windowControls.background : "transparent"

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.windowControls.spacing

        Repeater {
            model: root.buttons

            Rectangle {
                id: button
                required property string modelData
                readonly property bool close: modelData === "close"

                width: Theme.windowControls.size
                height: Theme.windowControls.size
                radius: Theme.windowControls.radius
                readonly property color hoverColor: close ? Theme.windowControls.closeHoverBackground
                                                          : Theme.windowControls.hoverBackground
                color: mouse.containsMouse ? hoverColor : Qt.alpha(hoverColor, 0)

                Behavior on color { enabled: Motion.fades; MotionColor {} }

                Icon {
                    anchors.centerIn: parent
                    size: Theme.windowControls.iconSize
                    color: button.close && mouse.containsMouse ? Theme.windowControls.closeHoverIcon
                                                               : Theme.windowControls.icon

                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                    path: button.modelData === "minimize" ? Icons.minimize
                        : button.modelData === "maximize" ? Icons.maximize
                        : Icons.close
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        switch (button.modelData) {
                        case "minimize": root.window.showMinimized(); break
                        case "maximize": root.maximized ? root.window.showNormal() : root.window.showMaximized(); break
                        case "close": root.dialog !== null ? root.dialog.close() : root.window.close(); break
                        }
                    }
                }

                ToolTip.visible: mouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: button.modelData === "minimize" ? qsTr("Minimize")
                            : button.modelData === "maximize" ? (root.maximized ? qsTr("Restore") : qsTr("Maximize"))
                            : qsTr("Close")
            }
        }
    }
}
