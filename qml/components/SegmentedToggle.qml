import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var choices: []
    property string current
    property string accessibleName

    signal picked(string id)

    readonly property real inset: Theme.layout.spacing / 2 + Theme.iconButton.borderWidth

    // Every segment as wide as the widest bold label, so checking one doesn't resize it.
    readonly property real segmentWidth: {
        let widest = 0
        for (let i = 0; i < probes.count; ++i)
            widest = Math.max(widest, probes.itemAt(i)?.implicitWidth ?? 0)
        return widest + 4 * Theme.layout.spacing
    }

    implicitWidth: root.choices.length * segmentWidth + 2 * inset
    implicitHeight: Theme.layout.toolbarHeight
    radius: Theme.iconButton.radius
    color: Theme.palette.surface
    border.color: Theme.iconButton.border
    border.width: Theme.iconButton.borderWidth
    Accessible.role: Accessible.RadioButton
    Accessible.name: accessibleName

    Repeater {
        id: probes
        model: root.choices
        Label {
            required property var modelData
            visible: false
            text: modelData.text
            font.weight: Theme.font.emphasisWeight
        }
    }

    // It springs in segments, not pixels, so a resize moves it without a bounce; hence the fine epsilon.
    Rectangle {
        readonly property int index: root.choices.findIndex(c => c.id === root.current)
        property real place: index

        visible: index >= 0 && root.choices.length > 0
        width: root.choices.length > 0 ? row.width / root.choices.length : 0
        x: row.x + place * width
        y: row.y
        height: row.height
        radius: Math.max(0, root.radius - root.inset + Theme.iconButton.borderWidth)
        color: Qt.alpha(Theme.segmented.checkedBackground, Theme.segmented.checkedOpacity)
        border.color: Qt.alpha(Theme.segmented.checkedText, 0.35)
        border.width: Theme.iconButton.borderWidth

        Behavior on place { enabled: Motion.on; MotionSpring { epsilon: 0.001 } }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: root.inset
        spacing: 0

        Repeater {
            model: root.choices

            Item {
                id: segment
                required property var modelData
                readonly property bool checked: root.current === modelData.id

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: root.segmentWidth

                Label {
                    anchors.centerIn: parent
                    text: segment.modelData.text
                    color: segment.checked ? Qt.lighter(Theme.segmented.checkedText, Theme.segmented.lightBrightness)
                                           : segmentMouse.containsMouse ? Theme.palette.text : Theme.segmented.text
                    font.weight: segment.checked ? Theme.font.emphasisWeight : Font.Normal
                }
                MouseArea {
                    id: segmentMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.picked(segment.modelData.id)
                    ToolTip.visible: containsMouse && (segment.modelData.tip ?? "") !== ""
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: segment.modelData.tip ?? ""
                }
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.text
                Accessible.checked: checked
            }
        }
    }
}
