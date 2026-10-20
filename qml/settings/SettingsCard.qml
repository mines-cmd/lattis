import QtQuick
import QtQuick.Layouts

Surface {
    default property alias rows: column.data
    property alias spacing: column.spacing
    property bool panel: false

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + (Theme.settingsWindow.cardPadding + Theme.settingsWindow.rowPadding) * 2
    radius: Theme.settingsWindow.cardRadius
    fill: panel ? Theme.onboarding.panelBackground : Theme.settingsWindow.cardBackground
    fillEnd: panel ? Theme.onboarding.panelBackgroundEnd : Theme.settingsWindow.cardBackgroundEnd
    direction: panel ? Theme.onboarding.gradientDirection : Theme.settingsWindow.gradientDirection
    border.color: panel ? Theme.onboarding.panelBorder : Theme.settingsWindow.cardBorder
    border.width: panel ? 1 : Theme.settingsWindow.cardBorderWidth

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.cardPadding + Theme.settingsWindow.rowPadding
        spacing: Theme.layout.spacing
    }
}
