import QtQuick
import QtQuick.Controls.Basic

SettingsMenu {
    id: root
    atPointer: true

    required property var hero

    component PositionAction: Action {
        property string position
        checkable: true
        checked: root.hero.position === position && root.hero.offsetX === 0 && root.hero.offsetY === 0
        onTriggered: {
            root.hero.setPosition(position)
            // triggering toggled it
            checked = Qt.binding(() => root.hero.position === position && root.hero.offsetX === 0 && root.hero.offsetY === 0)
        }
    }

    checkColumn: true

    function openBehindLogo(withSide) {
        const open = (menu, submenu) => {
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i)
                if (item?.subMenu === submenu) {
                    menu.currentIndex = i
                    submenu.popup(item, item.width, 0)
                }
            }
        }
        open(root, behindMenu)
        if (withSide)
            open(behindMenu, sideMenu)
    }

    Action {
        text: qsTr("Adjust logo")
        enabled: !root.hero.logoOnly
        onTriggered: root.hero.startAdjust()
    }
    SettingsMenu {
        id: behindMenu
        pops: true
        title: qsTr("Behind logo")
        checkColumn: true

        component StyleItem: SettingsMenuItem {
            property string style
            checked: root.hero.scrimChoice === style
            keepOpen: true
            onActivated: root.hero.setScrimStyle(style)
        }
        component SideItem: SettingsMenuItem {
            property string side
            checked: root.hero.scrimSideChoice === side
            keepOpen: true
            onActivated: root.hero.setScrimSide(side)
        }

        StyleItem { style: ""; text: qsTr("Theme default") }
        StyleItem { style: "gradient"; text: qsTr("Gradient") }
        StyleItem { style: "shadow"; text: qsTr("Shadow") }
        StyleItem { style: "none"; text: qsTr("None") }
        SettingsMenuSeparator {}
        SettingsMenu {
            id: sideMenu
            pops: true
            title: qsTr("Side")
            checkColumn: true

            SideItem { side: ""; text: qsTr("Theme default") }
            SideItem { side: "auto"; text: qsTr("Auto") }
            SideItem { side: "top"; text: qsTr("Top") }
            SideItem { side: "bottom"; text: qsTr("Bottom") }
            SideItem { side: "left"; text: qsTr("Left") }
            SideItem { side: "right"; text: qsTr("Right") }
            SideItem { side: "full"; text: qsTr("Full") }
        }
        SettingsMenuSlider {
            text: qsTr("Strength")
            enabled: root.hero.scrimStyle !== "none"
            from: 0
            to: 2
            value: root.hero.scrimStrength
            onChanged: value => root.hero.previewScrimStrength(value)
            onCommitted: value => root.hero.saveScrimStrength(value)
        }
        SettingsMenuSeparator {}
        SettingsMenuItem {
            text: qsTr("Reset to theme")
            enabled: root.hero.scrimCustom
            keepOpen: true
            onActivated: root.hero.resetScrim()
        }

        // The side only means something to a gradient.
        Component.onCompleted: {
            for (let i = 0; i < count; ++i) {
                const item = itemAt(i)
                if (item?.subMenu === sideMenu)
                    item.enabled = Qt.binding(() => root.hero.scrimStyle === "gradient")
            }
        }
    }
    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }
    }
    PositionAction { position: "top_left"; text: qsTr("Top left") }
    PositionAction { position: "top_center"; text: qsTr("Top center") }
    PositionAction { position: "top_right"; text: qsTr("Top right") }
    PositionAction { position: "center_left"; text: qsTr("Center left") }
    PositionAction { position: "center"; text: qsTr("Center") }
    PositionAction { position: "center_right"; text: qsTr("Center right") }
    PositionAction { position: "bottom_left"; text: qsTr("Bottom left") }
    PositionAction { position: "bottom_center"; text: qsTr("Bottom center") }
    PositionAction { position: "bottom_right"; text: qsTr("Bottom right") }
    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }
    }
    Action {
        text: qsTr("Reset size")
        enabled: (root.hero.game.logoScale ?? 1) !== 1
        onTriggered: root.hero.saveScale(1)
    }
    Action {
        text: qsTr("Reset position")
        enabled: (root.hero.game.logoPosition ?? "") !== "" || (root.hero.game.logoOffsetX ?? 0) !== 0
                 || (root.hero.game.logoOffsetY ?? 0) !== 0
        onTriggered: root.hero.resetPosition()
    }
}
