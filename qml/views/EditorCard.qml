import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    property alias title: heading.text
    property alias actions: actionsSlot.data
    default property alias fields: cardColumn.data

    Layout.fillWidth: true
    Layout.fillHeight: false
    spacing: Theme.gameEditor.cardTitleSpacing

    RowLayout {
        visible: heading.text !== ""
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            id: heading
            Layout.fillWidth: true
            color: Theme.gameEditor.cardTitleColor
            font.pixelSize: Theme.gameEditor.cardTitleFontSize
            font.weight: Theme.font.strongWeight
        }
        Item {
            id: actionsSlot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
    Surface {
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + 2 * Theme.gameEditor.cardPadding
        radius: Theme.gameEditor.cardRadius
        fill: Theme.gameEditor.cardBackground
        fillEnd: Theme.gameEditor.cardBackgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.cardBorder
        border.width: Theme.gameEditor.cardBorderWidth

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Theme.gameEditor.cardPadding
            spacing: Theme.gameEditor.rowSpacing
        }
    }
}
