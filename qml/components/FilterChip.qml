import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Surface {
    id: root

    property string text
    property color tagColor: "transparent"
    readonly property bool isTag: tagColor.a > 0

    signal clicked()
    signal removed()

    implicitWidth: label.implicitWidth + close.width + 4 + 2 * Theme.filters.chipPadding
    implicitHeight: Theme.filters.chipHeight
    radius: Theme.filters.chipRadius
    fill: isTag ? tagColor
                : body.containsMouse ? Qt.lighter(Theme.filters.chipBackground, Theme.filters.chipHoverBrightness)
                                     : Theme.filters.chipBackground
    fillEnd: isTag ? tagColor
                   : body.containsMouse ? Qt.lighter(Theme.filters.chipBackgroundEnd, Theme.filters.chipHoverBrightness)
                                        : Theme.filters.chipBackgroundEnd
    direction: Theme.filters.gradientDirection

    Behavior on fill { enabled: Motion.fades; MotionColor {} }
    Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

    MouseArea {
        id: body
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Label {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: Theme.filters.chipPadding
        text: root.text
        color: !root.isTag ? Theme.filters.chipText
             : Theme.tags.textStyle === "custom" ? Theme.tags.text
             : Qt.hsla(root.tagColor.hslHue, root.tagColor.hslSaturation,
                       Theme.tags.textStyle === "light" ? Theme.tags.lightTextLightness : Theme.tags.darkTextLightness, 1)
        font.pixelSize: Theme.filters.chipFontSize
        font.weight: !root.isTag ? Font.DemiBold
                   : Theme.tags.fontWeight === "thin" ? Font.Thin
                   : Theme.tags.fontWeight === "semibold" ? Font.DemiBold
                   : Theme.tags.fontWeight === "bold" ? Font.Bold : Font.Normal
    }

    Icon {
        id: close
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Theme.filters.chipPadding - 3
        size: Theme.filters.chipFontSize + 3
        path: Icons.close
        color: label.color
        opacity: closeMouse.containsMouse ? 1 : 0.7

        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.removed()
        }

        ToolTip.visible: closeMouse.containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: qsTr("Turn off")
    }
}
