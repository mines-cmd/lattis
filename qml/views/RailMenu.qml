import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
SettingsMenu {
    id: root

    signal platformHidden(string id)
    signal message(string text)

    pops: true
    checkColumn: true

    readonly property var platforms: Library.platformSettings.filter(p => p.games > 0)

    // The platform rows may not be inserted yet when the button rows are, so a count can't place them.
    function positionOf(item) {
        for (let i = 0; i < count; ++i)
            if (itemAt(i) === item)
                return i
        return count - 1
    }

    component ButtonRow: MenuCheckRow {
        property string name
        checked: !General.railHiddenButtons.includes(name)
        onToggled: {
            const error = General.setRailButtonShown(name, !checked)
            if (error !== "")
                root.message(error)
        }
    }

    Instantiator {
        // A count, not the list: a toggle reloads the platforms, and rebuilding the rows under the
        // pointer would drop the hover.
        model: root.platforms.length
        delegate: MenuCheckRow {
            required property int index
            readonly property var platform: root.platforms[index]
            text: platform.name
            checked: platform.inRail
            onToggled: {
                const id = platform.id
                const hiding = platform.inRail
                const error = Library.setPlatformValue(id, "in_rail", !hiding)
                if (error !== "")
                    root.message(error)
                else if (hiding)
                    root.platformHidden(id)
            }
        }
        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    SettingsMenuSeparator { id: platformsEnd; visible: root.platforms.length > 0 }
    Instantiator {
        model: General.railButtonOrder
        delegate: ButtonRow {
            required property string modelData
            name: modelData
            text: modelData === "add_game" ? qsTr("Add game")
                : modelData === "play_statistics" ? qsTr("Play statistics")
                : modelData === "gallery" ? qsTr("Gallery") : qsTr("Settings")
        }
        onObjectAdded: (index, object) => root.insertItem(root.positionOf(platformsEnd) + 1 + index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    SettingsMenuSeparator {}
    MenuCheckRow {
        text: qsTr("Show toolbar")
        checked: General.toolbar
        onToggled: {
            const error = General.setToolbar(!checked)
            if (error !== "")
                root.message(error)
        }
    }
}
