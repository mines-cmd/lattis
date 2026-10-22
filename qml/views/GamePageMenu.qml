import QtQuick
import QtQuick.Controls.Basic
import Qt.labs.qmlmodels

// See docs/code/qml.md.
SettingsMenu {
    id: root

    // chips, buttons or info_rows
    property string place: "chips"

    signal customizeRequested()
    signal message(string text)

    atPointer: true
    checkColumn: true

    readonly property var shown: place === "chips" ? General.gamePageChips
                               : place === "buttons" ? General.gamePageButtons : General.gamePageInfoRows

    // Every choice, hidden ones included. Replaced only when the order changes, so a toggle doesn't
    // rebuild the rows under the pointer.
    property var order: General.gamePageOrder(place)
    onPlaceChanged: order = General.gamePageOrder(place)

    Connections {
        target: General
        function onGamePageChanged() {
            const now = General.gamePageOrder(root.place)
            if (now.join(",") !== root.order.join(","))
                root.order = now
        }
    }

    GamePageNames { id: names }
    GameFields { id: fields }

    function report(error) {
        if (error !== "")
            message(error)
    }

    function toggle(name) {
        report(General.setGamePageShown(place, name, !shown.includes(name)))
    }

    component Entry: SettingsMenuItem {
        property string name
        readonly property bool isMenu: false
        text: names.forPlace(root.place)[name]
        checked: root.shown.includes(name)
        keepOpen: true
        onActivated: root.toggle(name)
    }

    // The period is picked in a submenu of the entry, which itself still toggles the field.
    component PeriodMenu: SettingsMenu {
        id: periodMenu
        readonly property bool isMenu: true
        title: names.fields.recent_play_time
        pops: true
        checkColumn: true

        Instantiator {
            model: ["week", "two_weeks", "month", "year"]
            delegate: SettingsMenuItem {
                required property string modelData
                text: fields.periodLabels[modelData]
                checked: General.recentPeriod === modelData
                keepOpen: true
                onActivated: root.report(General.setRecentPeriod(modelData))
            }
            onObjectAdded: (index, object) => periodMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => periodMenu.removeItem(object)
        }
    }

    Instantiator {
        model: root.order
        // A submenu isn't an Item, so it goes in with insertMenu().
        delegate: DelegateChooser {
            role: "modelData"
            DelegateChoice { roleValue: "recent_play_time"; PeriodMenu {} }
            DelegateChoice {
                Entry {
                    required property string modelData
                    name: modelData
                }
            }
        }
        onObjectAdded: (index, object) => {
            if (!object.isMenu) {
                root.insertItem(index, object)
                return
            }
            root.insertMenu(index, object)
            const item = root.itemAt(index)
            item.keepOpen = true
            item.checked = Qt.binding(() => root.shown.includes("recent_play_time"))
            item.activated.connect(() => root.toggle("recent_play_time"))
        }
        onObjectRemoved: (index, object) => {
            if (object.isMenu)
                root.removeMenu(object)
            else
                root.removeItem(object)
        }
    }
    SettingsMenuSeparator {}
    SettingsMenuItem {
        text: qsTr("Customize game page")
        onTriggered: root.customizeRequested()
    }
}
