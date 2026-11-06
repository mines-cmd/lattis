import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: header
    property string icon
    property alias title: titleLabel.text
    property alias tag: tagLabel.text
    property alias subtitle: subtitleLabel.text

    Layout.fillWidth: true
    spacing: 2

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Icon {
            path: header.icon
            size: Theme.controls.iconSize
            color: Theme.controls.descriptionColor
        }
        Label {
            id: titleLabel
            Layout.fillWidth: true
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            font.weight: Theme.font.strongWeight
        }
        SettingsNote { id: tagLabel }
    }
    SettingsNote {
        id: subtitleLabel
        visible: text !== ""
        Layout.fillWidth: true
        Layout.leftMargin: Theme.controls.iconSize + Theme.layout.spacing
    }
}
