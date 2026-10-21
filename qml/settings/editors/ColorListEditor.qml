import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GridLayout {
    id: root

    property string section
    property string key
    property var value

    readonly property int cellWidth: Math.floor((width - columnSpacing * (columns - 1)) / columns)

    columns: Math.max(1, Theme.colorPicker.paletteColumns)
    columnSpacing: 6
    rowSpacing: 6

    function commit(list) { Theme.setValue(root.section, root.key, list) }
    function formatHex(c) {
        const toHex = n => Math.round(n * 255).toString(16).padStart(2, "0")
        let hex = "#" + toHex(c.r) + toHex(c.g) + toHex(c.b)
        if (c.a < 1)
            hex += toHex(c.a)
        return hex
    }
    function replaceAt(i, c) {
        const list = root.value.slice()
        list[i] = c
        commit(list)
    }
    function move(i, delta) {
        const list = root.value.slice()
        const j = i + delta
        if (j < 0 || j >= list.length)
            return
        const tmp = list[i]
        list[i] = list[j]
        list[j] = tmp
        commit(list)
    }
    function removeAt(i) {
        const list = root.value.slice()
        list.splice(i, 1)
        commit(list)
    }

    Repeater {
        id: entries
        // A count, not the list itself: with the list as the model every edit would rebuild
        // the cells and close the picker mid-drag.
        model: root.value ? root.value.length : 0

        ColumnLayout {
            id: entry
            required property int index
            readonly property color modelData: root.value[index]

            onModelDataChanged: if (!field.activeFocus) field.text = root.formatHex(modelData)

            Layout.preferredWidth: root.cellWidth
            spacing: 4

            function openPicker() { picker.openFor(swatch, entry.modelData) }

            Rectangle {
                id: swatch
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.colorPicker.swatchSize
                radius: Theme.controls.inputRadius
                color: entry.modelData
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth > 0 ? Theme.controls.inputBorderWidth : 1

                MouseArea {
                    id: swatchMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            menu.popup()
                        else
                            picker.openFor(swatch, entry.modelData)
                    }
                }

                ToolTip.visible: swatchMouse.containsMouse && !picker.visible && !menu.visible
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Click to change or remove; right-click to move")

                ColorPickerPopup {
                    id: picker
                    removable: true
                    onColorPicked: c => root.replaceAt(entry.index, c)
                    onReferencePicked: (reference, c) => root.replaceAt(entry.index, c)
                    onRemoveRequested: root.removeAt(entry.index)
                }

                SettingsMenu {
                    id: menu
                    atPointer: true
                    Action { text: qsTr("Edit color"); onTriggered: picker.openFor(swatch, entry.modelData) }
                    Action { text: qsTr("Move left"); enabled: entry.index > 0; onTriggered: root.move(entry.index, -1) }
                    Action { text: qsTr("Move right"); enabled: entry.index < root.value.length - 1; onTriggered: root.move(entry.index, 1) }
                    Action { text: qsTr("Remove"); onTriggered: root.removeAt(entry.index) }
                }
            }

            SettingsTextField {
                id: field
                Layout.fillWidth: true
                leftPadding: 4
                rightPadding: 4
                horizontalAlignment: TextInput.AlignHCenter
                font.pixelSize: Theme.controls.descriptionFontSize
                Component.onCompleted: text = root.formatHex(entry.modelData)

                onEditingFinished: {
                    const t = text.trim().toLowerCase()
                    const current = root.formatHex(entry.modelData)
                    if (t === current)
                        return // not edited; saving would add the key to the theme file
                    if (/^#([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/.test(t))
                        root.replaceAt(entry.index, t)
                    else
                        text = current
                }
            }
        }
    }

    IconButton {
        id: addButton
        Layout.preferredWidth: root.cellWidth
        Layout.preferredHeight: Theme.colorPicker.swatchSize
        Layout.alignment: Qt.AlignTop
        icon: Icons.add
        iconSize: 14
        tip: qsTr("Add a color")
        // Adds a copy of the last color, then opens the new swatch's picker: the picker stays
        // with its swatch while this button moves along to the next cell.
        onClicked: {
            const last = root.value.length > 0 ? root.value[root.value.length - 1] : Qt.color("#808080")
            root.commit(root.value.concat([last]))
            Qt.callLater(() => entries.itemAt(entries.count - 1)?.openPicker())
        }
    }
}
