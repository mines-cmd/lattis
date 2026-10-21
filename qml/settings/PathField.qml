import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root

    property string path
    property string missingText
    property color missingColor: Theme.controls.descriptionColor
    property int fontSize: Theme.controls.pathFontSize

    spacing: Theme.layout.spacing

    signal copied()

    Rectangle {
        id: box
        visible: root.path !== ""
        Layout.fillWidth: true
        implicitHeight: Theme.controls.inputHeight
        radius: Theme.controls.inputRadius
        color: Theme.controls.inputBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.margins: box.border.width
            spacing: 0

            Label {
                id: pathLabel
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                readonly property string home: StandardPaths.writableLocation(StandardPaths.HomeLocation)
                                                   .toString().replace(/^file:\/\//, "")
                text: root.path.startsWith(home + "/") ? "~" + root.path.slice(home.length) : root.path
                color: Theme.controls.inputText
                font.family: Theme.font.monoFamily
                font.pixelSize: root.fontSize
                elide: Text.ElideMiddle

                HoverHandler { id: pathHover }
                ToolTip.visible: pathHover.hovered && pathLabel.truncated
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: root.path
            }
            Rectangle {
                Layout.preferredWidth: Math.max(1, Theme.controls.inputBorderWidth)
                Layout.preferredHeight: box.height / 2
                color: Theme.controls.inputBorder
            }
            AbstractButton {
                id: copyButton
                property bool copied: false
                Layout.fillHeight: true
                Layout.preferredWidth: box.height
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: copied ? qsTr("Copied") : qsTr("Copy the full path")
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                background: null
                property bool copyShown: true
                property bool markShown: false
                onCopiedChanged: {
                    if (copied) copyShown = false
                    else markShown = false
                    handoff.interval = Motion.duration(Motion.speedMs("normal"))
                    handoff.restart()
                }
                Timer {
                    id: handoff
                    onTriggered: {
                        if (copyButton.copied) copyButton.markShown = true
                        else copyButton.copyShown = true
                    }
                }
                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        path: Icons.copy
                        size: Theme.controls.iconSize
                        color: copyButton.hovered ? Theme.controls.inputText
                                                  : Theme.controls.descriptionColor
                        opacity: copyButton.copyShown ? 1 : 0

                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "normal" } }
                    }
                    MarkPop {
                        anchors.centerIn: parent
                        width: Theme.controls.iconSize
                        height: width
                        shown: copyButton.markShown
                        speed: "normal"
                        popSpeed: "normal"
                        hiddenScale: 0
                        overshoot: Theme.motion.markOvershoot * 2

                        Icon {
                            anchors.fill: parent
                            path: Icons.check
                            size: Theme.controls.iconSize
                            color: Theme.controls.inputText
                        }
                    }
                }
                onClicked: {
                    clipboard.text = root.path
                    clipboard.selectAll()
                    clipboard.copy()
                    copied = true
                    copiedTimer.restart()
                    root.copied()
                }
                Timer {
                    id: copiedTimer
                    interval: 1500
                    onTriggered: copyButton.copied = false
                }
            }
        }
    }
    Label {
        visible: root.path === ""
        Layout.fillWidth: true
        text: root.missingText
        color: root.missingColor
    }

    // Copies through a hidden text field: QML has no clipboard API of its own.
    TextEdit {
        id: clipboard
        visible: false
    }
}
