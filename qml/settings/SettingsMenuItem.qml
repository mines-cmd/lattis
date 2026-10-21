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

    implicitHeight: visible ? Theme.menu.itemHeight : 0
    leftPadding: Theme.menu.itemPadding
    rightPadding: Theme.menu.itemPadding
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
        spacing: item.iconColumnActive ? Theme.menu.iconSpacing : Theme.menu.itemSpacing
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
                    color: Theme.controls.inputText
                }
            }
        }
        Item {
            visible: item.iconColumnActive
            implicitWidth: Theme.menu.iconSize
            Layout.fillHeight: true
            Icon {
                anchors.centerIn: parent
                visible: item.effectiveIconPath !== ""
                path: item.effectiveIconPath || Icons.check
                size: Theme.menu.iconSize
                color: item.danger ? Theme.menu.dangerText : Theme.menu.iconColor
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: item.text
            color: item.danger ? Theme.menu.dangerText : Theme.controls.inputText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Label {
            visible: item.keys !== ""
            Layout.leftMargin: Theme.menu.keysSpacing
            Layout.fillHeight: true
            text: item.keys
            color: Theme.menu.keysColor
            verticalAlignment: Text.AlignVCenter
        }
        Icon {
            visible: item.subMenu !== null
            Layout.leftMargin: Theme.menu.keysSpacing
            path: Icons.chevronRight
            size: 16
            color: Theme.menu.keysColor
        }
    }
    background: Rectangle {
        radius: Theme.menu.itemRadius
        color: item.highlighted ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)

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
