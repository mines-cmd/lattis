import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AbstractButton {
    id: chip

    property bool selected: false
    property string detail
    property bool leftAligned: detail !== ""
    property string tip
    property bool showCheck: false

    Item {
        id: dip
        property real press: chip.pressed ? Theme.motion.pressScale : 1
        Behavior on press { enabled: Motion.on; MotionNumber {} }
        Binding { target: chip; property: "scale"; value: dip.press }
    }

    implicitHeight: Theme.filters.sortHeight
    leftPadding: Theme.layout.spacing
    rightPadding: Theme.layout.spacing
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    ToolTip.visible: hovered && (chip.tip !== "" || nameLabel.truncated)
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: chip.tip !== "" ? chip.tip : chip.text

    background: Rectangle {
        radius: Theme.filters.sortRadius
        color: chip.selected ? Theme.filters.sortSelected
             : chip.hovered ? Theme.filters.sortHover : Theme.filters.sortBackground
    }
    contentItem: RowLayout {
        spacing: Theme.layout.spacing

        Label {
            id: nameLabel
            Layout.fillWidth: true
            text: chip.text
            horizontalAlignment: chip.leftAligned ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideRight
            font.weight: chip.selected ? Theme.font.onAccentWeight : Font.Normal
            color: chip.selected ? Theme.filters.sortSelectedText : Theme.filters.hintColor
        }
        Label {
            visible: chip.detail !== ""
            text: chip.detail
            font.weight: Theme.font.emphasisWeight
            color: Theme.filters.hintColor
            opacity: chip.selected ? 0.75 : 1
            // On the selected color the muted text would vanish, so it follows the selected text instead.
            Binding on color { when: chip.selected; value: Theme.filters.sortSelectedText }
        }
        MarkPop {
            shown: chip.showCheck && chip.selected
            implicitWidth: checkIcon.implicitWidth
            implicitHeight: checkIcon.implicitHeight
            Icon {
                id: checkIcon
                path: Icons.check
                size: Theme.filters.headingFontSize + 3
                color: Theme.filters.sortSelectedText
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Theme.filters.sortRadius + 3
        color: "transparent"
        border.color: Theme.filters.focusRing
        border.width: Theme.filters.focusRingWidth
        visible: chip.visualFocus
    }
}
