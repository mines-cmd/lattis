import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    property alias title: sectionTitle.text
    property alias actions: sectionActions.data
    property alias note: sectionNote.text
    default property alias rows: sectionRows.data

    Layout.fillWidth: true
    Layout.preferredWidth: 1
    Layout.alignment: Qt.AlignTop
    spacing: 6

    RowLayout {
        Layout.fillWidth: true

        SectionTitle {
            id: sectionTitle
            Layout.fillWidth: true
        }
        Item {
            id: sectionActions
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: sectionRows.implicitHeight
        radius: Theme.controls.inputRadius
        color: Theme.controls.inputBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        ColumnLayout {
            id: sectionRows
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0
        }
    }
    Label {
        id: sectionNote
        visible: text !== ""
        Layout.fillWidth: true
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }
}
