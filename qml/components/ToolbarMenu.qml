import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    property var known: ({})

    signal message(string text)

    pops: true
    checkColumn: true

    function report(error) {
        if (error !== "")
            message(error)
    }

    Instantiator {
        model: General.toolbarButtonOrder.filter(name => root.known[name] !== undefined)
        delegate: MenuCheckRow {
            id: row
            required property string modelData
            text: root.known[modelData].tip
            checked: !General.toolbarHiddenButtons.includes(modelData)
            onToggled: {
                const shown = General.toolbarButtonOrder.filter(name => root.known[name] !== undefined
                                                                        && !General.toolbarHiddenButtons.includes(name))
                // The ticks stay as they were, so turning the toolbar on again brings the last button back.
                if (checked && shown.length === 1)
                    root.report(General.setToolbar(false))
                else
                    root.report(General.setToolbarButtonShown(modelData, !checked))
            }
        }
        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    SettingsMenuSeparator {}
    SettingsMenuItem {
        text: qsTr("Hide toolbar")
        onTriggered: root.report(General.setToolbar(false))
    }
}
