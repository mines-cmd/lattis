import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property alias text: label.text
    property string count
    // A name from `[game_page] sections`; the state is saved in `collapsed_sections`.
    property string section
    readonly property bool collapsed: General.collapsedSections.includes(section)

    signal failed(string text)

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Theme.details.collapseChevronGap

        Label {
            id: label
            color: Theme.details.headingColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.details.headingFontSize
        }
        Label {
            visible: root.count !== ""
            text: root.count
            color: Theme.details.countColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.details.headingFontSize
        }
        Icon {
            visible: Theme.details.collapseChevronSize > 0
            path: Icons.chevron
            size: Theme.details.collapseChevronSize
            color: Theme.details.collapseChevronColor
            opacity: area.containsMouse || root.collapsed ? 1 : 0
            rotation: root.collapsed ? 0 : 90
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
            Behavior on rotation { enabled: Motion.on; MotionNumber {} }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            const failure = General.setSectionCollapsed(root.section, !root.collapsed)
            if (failure !== "")
                root.failed(failure)
        }
        ToolTip.visible: containsMouse
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: root.collapsed ? qsTr("Expand") : qsTr("Collapse")
    }
}
