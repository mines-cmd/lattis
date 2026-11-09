import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    // Tabs are { id, name, count }; a tab without a count shows only its name.
    property var model: []
    property string current

    signal picked(string id)

    implicitHeight: row.implicitHeight + Theme.collections.editTabIndicatorHeight
    activeFocusOnTab: false

    function focusTab(i) {
        const n = model.length
        const item = repeater.itemAt((i + n) % n)
        if (item)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.collections.editTabIndicatorHeight
        color: Theme.collections.editSectionRule
    }

    RowLayout {
        id: row
        anchors.left: parent.left
        spacing: Theme.layout.spacing

        Repeater {
            id: repeater
            model: root.model

            AbstractButton {
                id: tab
                required property var modelData
                required property int index
                readonly property bool on: modelData.id === root.current

                implicitHeight: Theme.filters.sortHeight
                implicitWidth: label.implicitWidth + (count.visible ? count.implicitWidth + Theme.layout.spacing / 2 : 0)
                    + 2 * Theme.layout.spacing
                // One tab stop for the row: the current tab; arrows move to the others and the page follows.
                focusPolicy: on ? Qt.StrongFocus : Qt.ClickFocus
                hoverEnabled: true
                onClicked: root.picked(modelData.id)
                onActiveFocusChanged: if (activeFocus && !on) root.picked(modelData.id)
                Keys.onLeftPressed: root.focusTab(index - 1)
                Keys.onRightPressed: root.focusTab(index + 1)
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.name

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Item {
                    Rectangle {
                        anchors.fill: parent
                        anchors.bottomMargin: Theme.collections.editTabIndicatorHeight
                        radius: Theme.filters.sortRadius
                        color: Theme.filters.sortHover
                        visible: tab.hovered && !tab.on
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: Theme.collections.editTabIndicatorHeight
                        color: Theme.collections.editTabIndicator
                        visible: tab.on
                    }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: Theme.filters.sortRadius
                        color: "transparent"
                        border.color: Theme.filters.focusRing
                        border.width: Theme.filters.focusRingWidth
                        visible: tab.visualFocus
                    }
                }
                // The count is a label of its own, so a tab is found by its name alone.
                contentItem: Item {
                    Label {
                        id: label
                        x: Theme.layout.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.modelData.name
                        font.weight: tab.on ? Theme.font.emphasisWeight : Font.Normal
                        color: tab.on ? Theme.collections.editTabActiveText : Theme.collections.editTabText
                    }
                    Label {
                        id: count
                        visible: tab.modelData.count !== undefined
                        anchors.left: label.right
                        anchors.leftMargin: Theme.layout.spacing / 2
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.modelData.count ?? ""
                        color: Theme.collections.editTabCount
                    }
                }
            }
        }
    }
}
