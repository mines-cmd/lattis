import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MenuItem {
    id: item

    property real value: 0
    property real from: 0
    property real to: 1
    property real step: 0.1
    property var format: v => Math.round(v * 100) + "%"

    signal changed(real value)
    signal committed(real value)

    function nudge(direction) {
        const next = Math.max(from, Math.min(to, Math.round((value + direction * step) / step) * step))
        if (next === value)
            return
        changed(next)
        pause.restart()
    }

    implicitHeight: visible ? Theme.menu.itemHeight : 0
    leftPadding: Theme.menu.itemPadding
    rightPadding: Theme.menu.itemPadding
    opacity: enabled ? 1 : 0.45

    // Like SettingsMenuItem: while another entry's submenu is open (Side), the pointer on its way there
    // must not close it, so this row takes hover only once the pointer rests on it or isn't heading for
    // the submenu.
    readonly property bool blocked: (item.menu?.openSubmenuItem ?? null) !== null
    hoverEnabled: !blocked
    HoverHandler {
        onHoveredChanged: if (!hovered) rest.stop()
        onPointChanged: {
            if (!hovered || !item.menu)
                return
            const p = point.scenePosition
            if (item.blocked) {
                if (item.menu.headingToSubmenu(p))
                    rest.restart()
                else
                    item.takeOver()
            }
            item.menu.lastPointer = p
        }
    }
    Timer {
        id: rest
        interval: 300
        onTriggered: item.takeOver()
    }
    function takeOver() {
        const menu = item.menu
        menu.openSubmenuItem?.subMenu?.close()
        menu.openSubmenuItem = null
        for (let i = 0; i < menu.count; ++i) {
            if (menu.itemAt(i) === item)
                menu.currentIndex = i
        }
    }

    // Clicking, Enter or Space would trigger the entry and close the menu.
    Keys.onLeftPressed: event => { nudge(-1); event.accepted = true }
    Keys.onRightPressed: event => { nudge(1); event.accepted = true }
    Keys.onReturnPressed: event => event.accepted = true
    Keys.onEnterPressed: event => event.accepted = true
    Keys.onSpacePressed: event => event.accepted = true

    Timer {
        id: pause
        interval: 400
        onTriggered: item.committed(item.value)
    }

    indicator: null
    arrow: null
    contentItem: RowLayout {
        spacing: Theme.menu.itemSpacing
        Item {
            visible: item.menu?.checkColumn ?? false
            implicitWidth: 16
        }
        Label {
            text: item.text
            color: Theme.controls.inputText
            verticalAlignment: Text.AlignVCenter
            Layout.fillHeight: true
        }
        SettingsSlider {
            id: slider
            Layout.fillWidth: true
            Layout.minimumWidth: 110
            from: item.from
            to: item.to
            value: item.value
            focusPolicy: Qt.NoFocus
            onMoved: item.changed(value)
            onPressedChanged: {
                if (!pressed) {
                    pause.stop()
                    item.committed(value)
                }
            }
        }
        Label {
            text: item.format(item.value)
            color: Theme.menu.keysColor
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            Layout.preferredWidth: 40
            Layout.fillHeight: true
        }
    }
    background: Rectangle {
        radius: Theme.menu.itemRadius
        color: item.highlighted ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }
    // Takes the clicks beside the slider, which would trigger the entry and close the menu (see
    // SettingsMenuItem.keepOpen).
    MenuItem {
        anchors.fill: parent
        z: -0.5
        hoverEnabled: false
        background: null
        contentItem: Item {}
    }
}
