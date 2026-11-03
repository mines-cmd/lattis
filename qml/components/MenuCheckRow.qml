import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: row

    property string text
    property bool checked
    signal toggled()

    implicitHeight: Theme.menu.itemHeight
    implicitWidth: content.implicitWidth + 20

    Rectangle {
        anchors.fill: parent
        radius: Theme.menu.itemRadius
        color: hover.hovered ? Theme.controls.hover : "transparent"
    }
    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: Theme.menu.itemPadding
        anchors.rightMargin: Theme.menu.itemPadding
        spacing: Theme.menu.itemSpacing

        Item {
            implicitWidth: 16
            Layout.fillHeight: true
            Icon {
                anchors.centerIn: parent
                visible: row.checked
                path: Icons.check
                size: 16
                color: Theme.controls.inputText
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: row.text
            color: Theme.controls.inputText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        onTapped: row.toggled()
    }
}
