import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root

    property string text
    property color tint
    property bool filled: true
    property bool clickable: false
    property string tip

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
    implicitHeight: label.implicitHeight + 2
    radius: Theme.settingsWindow.sourceTagRadius
    color: filled ? Qt.alpha(tint, Theme.settingsWindow.sourceTagFillOpacity) : "transparent"
    border.color: tint
    border.width: Theme.settingsWindow.sourceTagBorderWidth

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.tint
        font.pixelSize: Theme.settingsWindow.sourceTagFontSize
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.clickable
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()

        ToolTip.visible: containsMouse && root.tip !== ""
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: root.tip
    }
}
