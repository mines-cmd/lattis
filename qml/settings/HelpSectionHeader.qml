import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    property alias text: title.text
    property alias description: descriptionLabel.text
    property string helpText
    property string helpUrl
    property string helpTip
    Layout.fillWidth: true
    Layout.topMargin: Theme.settingsWindow.sectionSpacing / 2
    Layout.bottomMargin: Theme.layout.spacing
    spacing: Theme.layout.spacing

    Label {
        id: title
        Layout.fillWidth: true
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.weight: Theme.font.strongWeight
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        SettingsNote {
            id: descriptionLabel
            Layout.fillWidth: true
        }
        AbstractButton {
            id: helpLink
            visible: helpText !== ""
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            ToolTip.visible: hovered
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: helpTip !== "" ? helpTip : helpUrl
            onClicked: Qt.openUrlExternally(helpUrl)
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            background: null
            contentItem: Row {
                spacing: 4

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    path: Icons.openExternal
                    size: Theme.controls.iconSize
                    color: helpLabel.color
                }
                Label {
                    id: helpLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: helpText
                    color: helpLink.hovered || helpLink.visualFocus ? Theme.controls.labelColor
                                                                    : Theme.controls.descriptionColor
                }
            }
        }
    }
}
