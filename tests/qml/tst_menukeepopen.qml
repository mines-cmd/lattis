import QtQuick
import QtQuick.Controls.Basic
import QtTest

// The pattern behind SettingsMenuItem.keepOpen: a direct MenuItem with an inner MenuItem on top of it.
// Qt closes the menu on a direct entry's `triggered`; the inner one isn't the menu's entry, so a click
// reaches only it, while the keyboard still moves through the outer entries.
Item {
    id: root
    width: 400
    height: 400
    property int activated: 0

    Menu {
        id: menu
        MenuItem { id: one; text: "one" }
        MenuItem {
            id: two
            text: "two"
            Keys.onReturnPressed: event => { root.activated++; event.accepted = true }
            MenuItem {
                anchors.fill: parent
                hoverEnabled: false
                background: null
                contentItem: Item {}
                onTriggered: root.activated++
            }
        }
        MenuItem { id: three; text: "three" }
    }

    TestCase {
        name: "MenuKeepOpen"
        when: windowShown

        function focused() {
            return one.activeFocus ? "one" : two.activeFocus ? "two" : three.activeFocus ? "three" : "none"
        }

        function test_clickKeepsMenuOpen() {
            root.activated = 0
            menu.popup(root, 10, 10)
            tryCompare(menu, "opened", true)
            const p = two.mapToItem(root, two.width / 2, two.height / 2)
            mouseClick(root, p.x, p.y)
            wait(100)
            compare(root.activated, 1)
            verify(menu.visible)
            menu.close()
        }

        function test_keyboardReachesEntryAndEnterKeepsMenuOpen() {
            root.activated = 0
            menu.popup(root, 10, 10)
            tryCompare(menu, "opened", true)
            const seen = []
            for (let i = 0; i < 3; ++i) {
                keyClick(Qt.Key_Down)
                wait(50)
                seen.push(focused())
            }
            compare(seen.join(","), "one,two,three")
            keyClick(Qt.Key_Up)
            wait(50)
            keyClick(Qt.Key_Return)
            wait(50)
            compare(root.activated, 1)
            verify(menu.visible)
            menu.close()
        }
    }
}
