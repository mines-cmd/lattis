import QtQuick
import QtQuick.Controls.Basic

// The ? that explains a label: sits right after it, and hovering either one shows the text.
// Hover comes from MouseAreas (a HoverHandler could stay hovered after the tooltip opened).
Item {
    id: root

    property Item target
    property string text
    property real iconSize: Theme.controls.descriptionFontSize
    property color color: Theme.controls.descriptionColor
    property color hoverColor: Theme.controls.labelColor
    readonly property bool hovered: iconMouse.containsMouse || targetMouse.containsMouse

    visible: text !== ""
    implicitWidth: icon.implicitWidth
    implicitHeight: icon.implicitHeight

    Icon {
        id: icon
        path: Icons.informationLine
        size: root.iconSize
        color: root.hovered ? root.hoverColor : root.color
    }
    MouseArea {
        id: iconMouse
        // Reaches back over the gap to the label so the pointer crossing it doesn't hide the tip.
        anchors.fill: parent
        anchors.leftMargin: -Theme.layout.spacingSmall
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
    MouseArea {
        id: targetMouse
        parent: root.target
        anchors.fill: parent
        enabled: root.visible
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
    ToolTip {
        visible: root.hovered
        delay: 0
        text: root.text
        width: Math.min(implicitWidth, Theme.gameEditor.helpWidth)
        // Its left edge at the ?, above it: the default placement centers it by its unwrapped
        // width, far to the left for a long text.
        x: 0
        y: -height - 4
    }
}
