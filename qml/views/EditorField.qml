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

        RowLayout {
            Layout.fillWidth: !marker.visible
            spacing: Theme.layout.spacingSmall

            Label {
                id: name
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
            HelpTip {
                target: name
                text: field.help
                iconSize: name.font.pixelSize
                color: Theme.gameEditor.helpColor
                hoverColor: Theme.gameEditor.helpHoverColor
            }
            Item { Layout.fillWidth: true }
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
        Item {
            id: actionsSlot
            visible: children.length > 0
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
