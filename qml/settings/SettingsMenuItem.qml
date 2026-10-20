import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MenuItem {
    id: item

    property string keys
    property bool danger: false
    // The entry doesn't close the menu: it takes the click, Enter and Space itself and emits `activated`
    // instead of `triggered`, which closes the menu.
    property bool keepOpen: false
    signal activated()
    property string iconPath: ""
    readonly property string effectiveIconPath: iconPath !== "" ? iconPath : (item.subMenu?.iconPath ?? "")
    readonly property bool iconColumnActive: item.menu?.iconColumn ?? false

    implicitHeight: visible ? Theme.settingsWindow.menuItemHeight : 0
    leftPadding: 10
    rightPadding: 10
    opacity: enabled ? 1 : 0.45
    // Qt closes an open submenu the moment the pointer touches another entry, so a diagonal path to
    // the submenu lost it. While another entry's submenu is open, this one takes hover only once the
    // pointer has rested on it or isn't heading for the submenu.
    readonly property bool submenuOpen: item.subMenu?.visible ?? false
    onSubmenuOpenChanged: {
        if (submenuOpen)
            item.menu.openSubmenuItem = item
        else if (item.menu?.openSubmenuItem === item)
            item.menu.openSubmenuItem = null
    }
    readonly property bool blocked: (item.menu?.openSubmenuItem ?? null) !== null && item.menu.openSubmenuItem !== item
    hoverEnabled: !blocked
    HoverHandler {
        id: pointer
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
        // Now, not when its exit transition ends: until then every move would take over again.
        menu.openSubmenuItem = null
        for (let i = 0; i < menu.count; ++i) {
            if (menu.itemAt(i) === item)
                menu.currentIndex = i
        }
        if (item.subMenu && item.enabled)
            item.subMenu.popup(item, item.width, 0)
    }

    Keys.onReturnPressed: event => { if (keepOpen) { activated(); event.accepted = true } else event.accepted = false }
    Keys.onEnterPressed: event => { if (keepOpen) { activated(); event.accepted = true } else event.accepted = false }
    Keys.onSpacePressed: event => { if (keepOpen) { activated(); event.accepted = true } else event.accepted = false }

    indicator: null
    arrow: null
    contentItem: RowLayout {
        spacing: item.iconColumnActive ? Theme.settingsWindow.menuIconSpacing : 6
        Item {
            visible: !item.iconColumnActive && (item.checkable || (item.menu?.checkColumn ?? false))
            implicitWidth: 16
            Layout.fillHeight: true
            MarkPop {
                anchors.fill: parent
                shown: item.checked

                Icon {
                    anchors.centerIn: parent
                    path: Icons.check
                    size: 16
                    color: Theme.settingsWindow.inputText
                }
            }
        }
        Item {
            visible: item.iconColumnActive
            implicitWidth: Theme.settingsWindow.menuIconSize
            Layout.fillHeight: true
            Icon {
                anchors.centerIn: parent
                visible: item.effectiveIconPath !== ""
                path: item.effectiveIconPath || Icons.check
                size: Theme.settingsWindow.menuIconSize
                color: item.danger ? Theme.settingsWindow.menuDangerText : Theme.settingsWindow.menuIconColor
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: item.text
            color: item.danger ? Theme.settingsWindow.menuDangerText : Theme.settingsWindow.inputText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Label {
            visible: item.keys !== ""
            Layout.leftMargin: 24
            Layout.fillHeight: true
            text: item.keys
            color: Theme.settingsWindow.menuKeysColor
            verticalAlignment: Text.AlignVCenter
        }
        Icon {
            visible: item.subMenu !== null
            Layout.leftMargin: 24
            path: Icons.chevronRight
            size: 16
            color: Theme.settingsWindow.menuKeysColor
        }
    }
    background: Rectangle {
        radius: Theme.settingsWindow.menuItemRadius
        color: item.highlighted ? Theme.settingsWindow.rowHover : Qt.alpha(Theme.settingsWindow.rowHover, 0)

        Behavior on color { enabled: Motion.fades; MotionColor {} }
    }
    // Qt closes the menu on any click that triggers a direct entry, and only a MenuItem that isn't one
    // takes the click from it.
    MenuItem {
        anchors.fill: parent
        enabled: item.keepOpen
        visible: item.keepOpen
        hoverEnabled: false
        background: null
        contentItem: Item {}
        onTriggered: item.activated()
    }
}
