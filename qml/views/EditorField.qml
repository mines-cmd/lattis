import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: field
    property alias label: name.text
    property bool marked: false
    property string markerText: qsTr("from IGDB")
    property string help
    property string labelTip
    property bool heading: false
    property alias actions: actionsSlot.data

    Layout.fillWidth: true
    spacing: Theme.gameEditor.labelSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            id: name
            Layout.fillWidth: !marker.visible
            color: field.heading ? Theme.gameEditor.cardTitleColor : Theme.gameEditor.labelColor
            font.weight: field.heading ? Theme.font.emphasisWeight : Font.Normal
            elide: Text.ElideRight

            MouseArea {
                id: nameMouse
                anchors.fill: parent
                enabled: field.labelTip !== ""
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
            ToolTip.visible: nameMouse.containsMouse
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: field.labelTip
        }
        Label {
            id: marker
            visible: field.marked
            Layout.fillWidth: true
            text: field.markerText
            color: Theme.gameEditor.markerColor
            font.pixelSize: Theme.gameEditor.markerFontSize
            elide: Text.ElideRight
        }
        // A control's own hover, like the buttons' tooltips: a HoverHandler's could stay
        // hovered after the tooltip opened.
        AbstractButton {
            id: help
            visible: field.help !== ""
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            background: null
            contentItem: Icon {
                path: Icons.help
                size: name.font.pixelSize
                color: help.hovered ? Theme.gameEditor.helpHoverColor : Theme.gameEditor.helpColor
            }

            ToolTip {
                visible: help.hovered
                delay: Theme.tooltip.delay
                text: field.help
                width: Math.min(implicitWidth, Theme.gameEditor.helpWidth)
                // Its right edge at the ?, above it. The default placement centers it by its
                // unwrapped width, far to the left for a long text.
                x: parent.width - width
                y: -height - 4
            }
        }
        Item {
            id: actionsSlot
            visible: children.length > 0
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
