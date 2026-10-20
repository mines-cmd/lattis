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

    component CheckRow: Item {
        id: row

        property string text
        property bool checked
        signal toggled()

        implicitHeight: Theme.settingsWindow.menuItemHeight
        implicitWidth: content.implicitWidth + 20

        Rectangle {
            anchors.fill: parent
            radius: Theme.settingsWindow.menuItemRadius
            color: hover.hovered ? Theme.settingsWindow.rowHover : "transparent"
        }
        RowLayout {
            id: content
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            Item {
                implicitWidth: 16
                Layout.fillHeight: true
                Icon {
                    anchors.centerIn: parent
                    visible: row.checked
                    path: Icons.check
                    size: 16
                    color: Theme.settingsWindow.inputText
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: row.text
                color: Theme.settingsWindow.inputText
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
        HoverHandler {
            id: hover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            onTapped: row.toggled()
        }
    }

    component ButtonRow: CheckRow {
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
        delegate: CheckRow {
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
    SettingsMenuSeparator { visible: root.platforms.length > 0 }
    Instantiator {
        model: General.railButtonOrder
        delegate: ButtonRow {
            required property string modelData
            name: modelData
            text: modelData === "add_game" ? qsTr("Add game")
                : modelData === "play_statistics" ? qsTr("Play statistics") : qsTr("Settings")
        }
        onObjectAdded: (index, object) => root.addItem(object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
}
