import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ToolTip {
    id: root

    property var item: null
    property url icon

    readonly property bool concealed: (item?.hidden ?? false) && !(item?.unlocked ?? false)
    readonly property real percent: item?.percent ?? -1

    padding: 0
    margins: 0
    delay: Theme.tooltip.delay
    enter: Transition { MotionNumber { property: "opacity"; from: 0; to: 1; fade: true } }
    exit: Transition { MotionNumber { property: "opacity"; to: 0; fade: true } }

    background: Rectangle {
        radius: Theme.radius.tiny
        color: Theme.tooltip.background
        border.color: Theme.tooltip.border
        border.width: Theme.tooltip.borderWidth
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitWidth: Theme.achievements.tipWidth
            implicitHeight: header.implicitHeight + 2 * Theme.achievements.tipPadding
            color: "transparent"

            // Rounded at the top only, in two pieces that don't overlap: the background is translucent, so an
            // overlap would show as a lighter strip.
            Item {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: Theme.radius.tiny
                clip: true

                Rectangle {
                    width: parent.width
                    height: Theme.radius.tiny * 2
                    radius: Theme.radius.tiny
                    color: Theme.achievements.tipHeaderBackground
                }
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom
                          topMargin: Theme.radius.tiny }
                color: Theme.achievements.tipHeaderBackground
            }

            RowLayout {
                id: header
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: Theme.achievements.tipPadding }
                spacing: Theme.achievements.tipGap

                ArtThumb {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: Theme.achievements.tipIconSize
                    implicitHeight: Theme.achievements.tipIconSize
                    aspect: 1
                    cornerRadius: Theme.achievements.iconRadius
                    color: Theme.achievements.iconPlaceholder
                    failedIcon: Icons.medal
                    iconWhenEmpty: !root.concealed
                    source: root.concealed ? "" : root.icon

                    RareGlow {
                        anchors.fill: parent
                        active: (root.item?.unlocked ?? false) && root.percent >= 0 && root.percent < Theme.achievements.rarePercent
                    }
                    Label {
                        visible: root.concealed
                        anchors.centerIn: parent
                        text: "?"
                        color: Theme.achievements.viewerHiddenMarkColor
                        font.pixelSize: parent.height * 0.5
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.fillWidth: true
                        text: root.concealed ? qsTr("Hidden achievement") : (root.item?.name ?? "")
                        wrapMode: Text.Wrap
                        color: Theme.achievements.tipNameColor
                        font.weight: Theme.achievements.tipNameWeight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: root.concealed ? qsTr("Details for this achievement will be revealed once you've unlocked it")
                                             : (root.item?.description ?? "")
                        wrapMode: Text.Wrap
                        color: Theme.achievements.tipDescriptionColor
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.achievements.tipPadding
            spacing: Theme.layout.spacing

            Label {
                text: root.item?.unlocked
                      ? qsTr("Unlocked %1").arg(root.item.time.toLocaleString(Qt.locale(), General.shortDateFormat + ", HH:mm"))
                      : qsTr("Locked")
                color: Theme.achievements.tipDateColor
            }
            Label {
                visible: root.percent >= 0
                text: qsTr("%1% of players have this achievement").arg(root.percent < 10 ? root.percent.toFixed(1) : Math.round(root.percent))
                color: root.percent < Theme.achievements.rarePercent ? Theme.achievements.tipRareColor
                                                                    : Theme.achievements.tipRarityColor
            }
        }
    }
}
