import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property var game: ({})

    readonly property var available: ({
        release_date: { label: qsTr("Release date"), value: game.year ?? "" },
        platform: { label: qsTr("Platform"), value: game.platformName ?? "", icon: Icons.forPlatform(game.platformIcon ?? ""), filterPlatform: game.platform ?? "" },
        status: { label: qsTr("Status"), value: game.statusText ?? "" },
        install_size: { label: qsTr("Install size"), value: game.sizeText ?? "" },
        rating: { label: qsTr("Rating"), value: (game.rating ?? -1) >= 0 ? game.rating.toFixed(1) : "", stars: game.rating ?? -1 }
    })
    readonly property var rows: General.gamePageInfoRows.filter(name => available[name] !== undefined).map(name => available[name]).filter(r => r.value !== "")

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

                readonly property bool clickable: (modelData.filterPlatform ?? "") !== ""
                readonly property color valueColor: !clickable ? Theme.infoPanel.valueColor
                    : platformClick.containsMouse ? Theme.infoPanel.valueHoverColor
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
                        scale: infoRow.clickable && platformClick.pressed ? Theme.motion.pressScale : 1
                        Behavior on scale { enabled: Motion.on; MotionNumber {} }

                        Icon {
                            visible: infoRow.modelData.icon !== undefined
                            path: infoRow.modelData.icon ?? ""
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
                    id: platformClick
                    anchors.fill: parent
                    enabled: infoRow.clickable
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    hoverEnabled: true
                    onClicked: Library.platformFilter = infoRow.modelData.filterPlatform

                    ToolTip {
                        id: tip
                        visible: platformClick.enabled && platformClick.containsMouse
                        delay: Theme.tooltip.delay
                        text: qsTr("Show only %1 games").arg(infoRow.modelData.value)
                        x: platformClick.width - width - Theme.infoPanel.rowPadding / 2
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
