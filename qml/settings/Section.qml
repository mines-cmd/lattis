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
        radius: Theme.settingsWindow.inputRadius
        color: Theme.settingsWindow.inputBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth

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
        color: Theme.settingsWindow.descriptionColor
        font.pixelSize: Theme.settingsWindow.descriptionFontSize
        wrapMode: Text.WordWrap
    }
}
