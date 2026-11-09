import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    // Rows are { id, title, info, enabled }; info is the muted text at the row's right, enabled
    // defaults to true. A disabled row can't be ticked and is drawn dimmed.
    property var model: []
    property var ticked: []
    property bool withSelectAll: false
    property int maxListHeight: Theme.collections.importListHeight

    signal toggled(string id)
    signal allToggled(bool on)

    readonly property var selectable: model.filter(r => r.enabled !== false).map(r => r.id)

    implicitHeight: column.implicitHeight + 2 * Theme.collections.checklistPadding
    radius: Theme.controls.inputRadius
    color: Theme.controls.inputBackground
    border.color: Theme.controls.inputBorder
    border.width: Theme.controls.inputBorderWidth

    component Tally: Label {
        anchors.right: parent.right
        anchors.rightMargin: Theme.collections.checklistRowPadding
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.collections.rowCountColor
        font.pixelSize: Theme.controls.labelFontSize
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.collections.checklistPadding
        spacing: 0

        SettingsCheckBox {
            id: selectAll
            readonly property int count: root.ticked.length
            visible: root.withSelectAll
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.collections.checklistRowHeight
            leftPadding: Theme.collections.checklistRowPadding
            rightPadding: Theme.collections.checklistRowPadding + tally.implicitWidth + spacing
            bordered: false
            enabled: root.selectable.length > 0
            text: qsTr("Select all")
            checked: count > 0 && count === root.selectable.length
            partial: count > 0 && count < root.selectable.length
            nextCheckState: () => {
                root.allToggled(!checked)
                return checkState
            }

            Tally {
                id: tally
                text: qsTr("%1 of %2 selected").arg(selectAll.count).arg(root.selectable.length)
            }
        }

        Rectangle {
            visible: root.withSelectAll
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.controls.inputBorder
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Math.max(1, Math.floor(root.maxListHeight / Theme.collections.checklistRowHeight)) * Theme.collections.checklistRowHeight)
            clip: true
            // A curve-rendered check mark in a row cut off by the edge isn't clipped, only a layer is.
            layer.enabled: interactive
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            model: root.model
            ScrollBar.vertical: SettingsScrollBar { id: bar; trackless: true }

            delegate: SettingsCheckBox {
                id: row
                required property var modelData
                readonly property bool off: modelData.enabled === false
                width: list.width - (bar.visible ? bar.thickness + Theme.layout.spacing : 0)
                height: Theme.collections.checklistRowHeight
                leftPadding: Theme.collections.checklistRowPadding
                rightPadding: Theme.collections.checklistRowPadding + info.implicitWidth + spacing
                bordered: off
                enabled: !off
                opacity: off ? 0.5 : 1
                text: modelData.title
                checked: root.ticked.includes(modelData.id)
                nextCheckState: () => {
                    root.toggled(modelData.id)
                    return checkState
                }

                background: Rectangle {
                    radius: Theme.collections.rowRadius
                    color: row.hovered ? Theme.collections.rowHover : "transparent"
                }

                Tally {
                    id: info
                    text: row.modelData.info ?? ""
                }
            }
        }
    }
}
