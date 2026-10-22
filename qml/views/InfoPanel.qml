import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    required property GameFields fields
    readonly property var rows: fields.shown(General.gamePageInfoRows).map(name => fields.all[name])

    signal historyRequested()

    implicitHeight: column.implicitHeight + 2 * Theme.infoPanel.padding
    radius: Theme.infoPanel.radius
    fill: Theme.infoPanel.background
    fillEnd: Theme.infoPanel.backgroundEnd
    direction: Theme.infoPanel.gradientDirection
    border.color: Theme.infoPanel.border
    border.width: Theme.infoPanel.borderWidth

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Theme.infoPanel.padding }
        spacing: 0

        Repeater {
            model: root.rows

            Item {
                id: infoRow

                required property var modelData
                required property int index

                readonly property bool clickable: (modelData.filterPlatform ?? "") !== "" || (modelData.history ?? false)
                readonly property color valueColor: !clickable ? Theme.infoPanel.valueColor
                    : rowClick.containsMouse ? Theme.infoPanel.valueHoverColor
                    : Theme.infoPanel.valueColor

                Layout.fillWidth: true
                implicitHeight: Theme.infoPanel.rowHeight

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.infoPanel.rowPadding
                    anchors.rightMargin: Theme.infoPanel.rowPadding
                    spacing: Theme.infoPanel.labelSpacing

                    Label {
                        Layout.fillWidth: true
                        text: infoRow.modelData.label
                        color: Theme.infoPanel.labelColor
                    }
                    RowLayout {
                        spacing: Theme.infoPanel.labelSpacing
                        transformOrigin: Item.Right
                        scale: infoRow.clickable && rowClick.pressed ? Theme.motion.pressScale : 1
                        Behavior on scale { enabled: Motion.on; MotionNumber {} }

                        Icon {
                            visible: infoRow.modelData.valueIcon ?? false
                            path: infoRow.modelData.icon
                            size: Theme.infoPanel.iconSize
                            color: infoRow.valueColor
                            Behavior on color { enabled: Motion.fades; MotionColor {} }
                        }
                        Label {
                            visible: infoRow.modelData.stars === undefined
                            text: infoRow.modelData.value
                            color: infoRow.valueColor
                            Behavior on color { enabled: Motion.fades; MotionColor {} }
                        }
                    }
                    Stars {
                        visible: infoRow.modelData.stars !== undefined
                        value: infoRow.modelData.stars ?? -1
                        starSize: Theme.infoPanel.starSize
                        spacing: Theme.infoPanel.starSpacing
                        color: Theme.infoPanel.star
                        emptyColor: Theme.infoPanel.starEmpty

                        HoverHandler {
                            id: starsHover
                        }
                        ToolTip.visible: starsHover.hovered && visible
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("%1 out of 5").arg((infoRow.modelData.stars ?? 0).toFixed(1))
                    }
                }

                MouseArea {
                    id: rowClick
                    anchors.fill: parent
                    enabled: infoRow.clickable
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    hoverEnabled: true
                    onClicked: {
                        if (infoRow.modelData.history ?? false)
                            root.historyRequested()
                        else
                            Library.platformFilter = infoRow.modelData.filterPlatform
                    }

                    ToolTip {
                        id: tip
                        visible: rowClick.enabled && rowClick.containsMouse
                        delay: Theme.tooltip.delay
                        text: infoRow.modelData.tip ?? ""
                        x: rowClick.width - width - Theme.infoPanel.rowPadding / 2
                        y: -height
                    }
                }

                Rectangle {
                    visible: Theme.infoPanel.showDividers && infoRow.index < root.rows.length - 1
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 10; rightMargin: 10 }
                    height: Theme.infoPanel.dividerWidth
                    color: Theme.infoPanel.divider
                }
            }
        }
    }
}
