import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Flow {
    id: root

    property var model: []
    property bool multiple: false
    // The picked id when one-of-n, the list of picked ids when `multiple`. The owner sets it
    // from picked(id) (the clicked chip's id, also when it was already on).
    property var current: multiple ? [] : ""

    signal picked(var id)

    spacing: Theme.layout.spacing
    activeFocusOnTab: false

    function isOn(id) {
        return multiple ? current.includes(id) : current === id
    }

    // One tab stop per row: the chip that is picked (else the first) takes Tab, arrows move among the rest.
    readonly property int tabStop: {
        for (let i = 0; i < model.length; i++) {
            if (isOn(model[i].id))
                return i
        }
        return 0
    }

    function focusChip(i) {
        const n = model.length
        const item = repeater.itemAt((i + n) % n)
        if (item)
            item.forceActiveFocus(Qt.TabFocusReason)
    }

    Repeater {
        id: repeater
        model: root.model

        ToggleChip {
            required property var modelData
            required property int index

            text: modelData.name
            selected: root.isOn(modelData.id)
            showCheck: true
            focusPolicy: index === root.tabStop ? Qt.StrongFocus : Qt.ClickFocus
            onClicked: root.picked(modelData.id)
            Keys.onLeftPressed: root.focusChip(index - 1)
            Keys.onRightPressed: root.focusChip(index + 1)
        }
    }
}
