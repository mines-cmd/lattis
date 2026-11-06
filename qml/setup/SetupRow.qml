import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    property alias label: name.text
    property alias note: noteLabel.text
    property alias noteColor: noteLabel.color
    default property alias controls: controlRow.data

    Layout.fillWidth: true
    spacing: Theme.layout.spacing * 1.5

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Label {
            id: name
            Layout.fillWidth: true
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            wrapMode: Text.Wrap
        }
        SettingsNote {
            id: noteLabel
            visible: text !== ""
            Layout.fillWidth: true
            linkColor: Theme.gameEditor.linkColor
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }
    RowLayout {
        id: controlRow
        spacing: Theme.layout.spacing
    }
}
