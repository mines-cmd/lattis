import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: root

    property bool radio: false
    property bool partial: false
    property bool showBox: true
    property bool showDot: false
    property color dot: "transparent"
    property color swatch: "transparent"
    property string count
    property bool selected: false
    property bool dimmed: false
    property bool highlighted: false
    property int textFormat: Text.PlainText

    hoverEnabled: true
    padding: 0
    topPadding: Theme.layout.spacing / 2
    bottomPadding: Theme.layout.spacing / 2
    leftPadding: Theme.layout.spacing
    rightPadding: Theme.layout.spacing
    implicitHeight: content.implicitHeight + topPadding + bottomPadding

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    Keys.onReturnPressed: clicked()

    background: Rectangle {
        radius: Theme.settingsWindow.rowRadius
        color: root.selected ? Theme.gameEditor.bulkSidebarSelected
             : root.hovered || root.highlighted ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)
        border.color: Theme.controls.inputBorderFocus
        border.width: root.visualFocus ? Theme.controls.inputBorderWidth : 0

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }

    contentItem: RowLayout {
        id: content
        spacing: Theme.layout.spacing
        opacity: root.dimmed ? Theme.gameEditor.readOnlyOpacity : 1
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }

        Rectangle {
            visible: root.showBox
            implicitWidth: Theme.gameEditor.checkSize
            implicitHeight: Theme.gameEditor.checkSize
            radius: root.radio ? width / 2 : Theme.controls.inputRadius / 2
            color: (root.checked || root.partial) && !root.radio ? Theme.gameEditor.checkOn : Theme.controls.inputBackground
            border.color: root.checked || root.partial ? Theme.gameEditor.checkOn : Theme.controls.inputBorder
            border.width: root.radio ? Theme.gameEditor.bulkRadioBorderWidth
                         : root.hovered && !root.checked ? Theme.controls.inputBorderWidth : 0

            Behavior on color { enabled: Motion.fades; MotionColor {} }
            Behavior on border.color { enabled: Motion.fades; MotionColor {} }

            BoxBump { value: root.checked }
            MarkPop {
                anchors.fill: parent
                shown: root.checked && !root.radio
                hiddenScale: 0
                overshoot: Theme.motion.markOvershoot * 1.3
                Icon {
                    anchors.centerIn: parent
                    path: Icons.check
                    size: parent.width - 4
                    color: Theme.gameEditor.checkMark
                }
            }
            MarkPop {
                anchors.fill: parent
                shown: root.partial && !root.checked && !root.radio
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width / 2
                    height: 2
                    radius: 1
                    color: Theme.gameEditor.checkMark
                }
            }
            // A whole-pixel gap all round: centering on a half pixel puts it off center.
            MarkPop {
                readonly property int inset: Math.round(parent.width / 4)
                x: inset
                y: inset
                shown: root.checked && root.radio
                width: parent.width - 2 * inset
                height: width
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Theme.gameEditor.checkOn
                }
            }
        }
        Item {
            visible: root.showDot
            implicitWidth: Theme.gameEditor.checkSize
            implicitHeight: Theme.gameEditor.checkSize
            Rectangle {
                readonly property int size: 2 * Math.round(Theme.gameEditor.checkSize / 4)
                x: Math.round((parent.width - size) / 2)
                y: Math.round((parent.height - size) / 2)
                width: size
                height: size
                radius: size / 2
                color: root.dot
            }
        }
        Rectangle {
            visible: root.swatch.a > 0
            implicitWidth: Theme.gameEditor.hintFontSize - 2
            implicitHeight: implicitWidth
            radius: Theme.controls.inputRadius / 4
            color: root.swatch
        }
        Label {
            Layout.fillWidth: true
            text: root.text
            textFormat: root.textFormat
            color: Theme.gameEditor.lookupText
            elide: Text.ElideRight
        }
        Label {
            visible: root.count !== ""
            text: root.count
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            font.features: { "tnum": 1 }
        }
    }
}
