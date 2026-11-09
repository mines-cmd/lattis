import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    // "new" makes a collection holding `ids` (none from the filter panel); "save" makes one from the
    // filters now on; "rename" renames `collectionId`; "edit" changes its rules and exceptions.
    property string mode: "new"
    property var ids: []
    property string collectionId
    property string startName
    signal message(string text)

    readonly property string typed: input.text.trim()
    readonly property bool renaming: mode === "rename"
    readonly property bool editing: mode === "edit"

    function finish(error) {
        close()
        if (error !== "")
            message(error)
    }

    function submit() {
        if (typed === "")
            return
        finish(renaming ? Library.renameCollection(collectionId, typed)
            : mode === "save" ? Library.addCollectionFromFilters(typed)
            : Library.addCollection(typed, ids))
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(editing ? Theme.collections.editWidth : Theme.gameEditor.confirmWidth,
                    parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height * Theme.gameEditor.maxHeight)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onOpened: {
        if (editing) {
            editForm.item.load()
            return
        }
        input.text = renaming ? startName : ""
        input.selectAll()
        input.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            visible: !root.editing
            Layout.fillWidth: true
            text: root.renaming ? qsTr("Rename collection")
                : root.mode === "save" ? qsTr("Save the filters as a collection")
                : root.ids.length === 0 ? qsTr("New collection")
                : qsTr("New collection for %n game(s)", "", root.ids.length)
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            wrapMode: Text.WordWrap
        }
        SettingsTextField {
            id: input
            visible: !root.editing
            Layout.fillWidth: true
            placeholderText: qsTr("Name of the collection")
            onAccepted: root.submit()
        }

        Loader {
            id: editForm
            active: root.editing
            visible: root.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "CollectionEditForm.qml"
            onLoaded: {
                item.collectionId = Qt.binding(() => root.collectionId)
                item.finished.connect(error => root.finish(error))
                item.cancelled.connect(() => root.close())
            }
        }

        RowLayout {
            visible: !root.editing
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                primary: true
                text: root.renaming ? qsTr("Rename") : qsTr("Add")
                enabled: root.typed !== ""
                onClicked: root.submit()
            }
        }
    }
}
