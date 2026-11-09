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

    property bool rulesOn: false
    property string platform
    property bool installedOnly: false
    property bool favoritesOnly: false
    property var statuses: []
    property var tags: []
    property bool matchAllTags: false
    property var steam: []
    property var added: []
    property var removed: []
    property var dropAdded: []
    property var dropRemoved: []

    readonly property var platformChoices: {
        const choices = [{ id: "", name: qsTr("Any platform") }]
        for (const p of Library.platforms)
            choices.push({ id: p.id, name: p.name })
        if (platform !== "" && !choices.some(c => c.id === platform))
            choices.push({ id: platform, name: platform })
        return choices
    }

    function toggled(list, value) {
        const next = list.slice()
        const i = next.indexOf(value)
        if (i >= 0)
            next.splice(i, 1)
        else
            next.push(value)
        return next
    }

    function load() {
        const d = Library.collectionDetails(collectionId)
        rulesOn = d.enabled ?? false
        platform = d.platform ?? ""
        installedOnly = d.installedOnly ?? false
        favoritesOnly = d.favoritesOnly ?? false
        statuses = d.statuses ?? []
        tags = d.tags ?? []
        matchAllTags = d.matchAllTags ?? false
        steam = d.steam ?? []
        added = d.added ?? []
        removed = d.removed ?? []
        dropAdded = []
        dropRemoved = []
    }

    function submit() {
        let error = ""
        if (editing) {
            error = Library.editCollection(collectionId, {
                enabled: rulesOn, platform: platform, installedOnly: installedOnly, favoritesOnly: favoritesOnly,
                statuses: statuses, tags: tags, matchAllTags: matchAllTags && tags.length >= 2, steam: steam
            }, dropAdded, dropRemoved)
        } else {
            if (typed === "")
                return
            error = renaming ? Library.renameCollection(collectionId, typed)
                : mode === "save" ? Library.addCollectionFromFilters(typed)
                : Library.addCollection(typed, ids)
        }
        close()
        if (error !== "")
            message(error)
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
            load()
            form.forceActiveFocus()
            return
        }
        input.text = renaming ? startName : ""
        input.selectAll()
        input.forceActiveFocus()
    }

    component Group: ColumnLayout {
        property string title
        Layout.fillWidth: true
        spacing: Theme.layout.spacing / 2

        Label {
            text: parent.title
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
        }
    }

    component Hint: Label {
        Layout.fillWidth: true
        color: Theme.gameEditor.hintColor
        wrapMode: Text.WordWrap
    }

    // A game that isn't in the library has only its id: Steam's are shown as the app they name.
    function gameRows(games) {
        return games.map(g => {
            const missing = g.title === g.id
            return {
                id: g.id,
                title: !missing ? g.title
                    : g.id.startsWith("steam-") ? qsTr("Steam app %1").arg(g.id.slice(6)) : g.id,
                info: missing ? qsTr("Not in your library") : ""
            }
        })
    }

    function kept(games, drops) {
        return games.map(g => g.id).filter(id => !drops.includes(id))
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: root.editing ? qsTr("Edit collection")
                : root.renaming ? qsTr("Rename collection")
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

        Flickable {
            id: form
            visible: root.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: formColumn.implicitHeight
            contentHeight: formColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { id: formBar; trackless: true }
            Keys.onEscapePressed: root.close()

            ColumnLayout {
                id: formColumn
                width: form.width - (formBar.visible ? formBar.thickness + Theme.layout.spacing : 0)
                spacing: Theme.gameEditor.rowSpacing

                SettingsCheckBox {
                    bordered: false
                    text: qsTr("Keep up to date with these rules")
                    checked: root.rulesOn
                    onToggled: root.rulesOn = checked
                }

                ColumnLayout {
                    visible: root.rulesOn
                    Layout.fillWidth: true
                    spacing: Theme.gameEditor.rowSpacing

                    SettingsComboBox {
                        Layout.fillWidth: true
                        model: root.platformChoices.map(c => c.name)
                        currentIndex: Math.max(0, root.platformChoices.findIndex(c => c.id === root.platform))
                        onActivated: index => root.platform = root.platformChoices[index].id
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing * 2

                        SettingsCheckBox {
                            bordered: false
                            text: qsTr("Installed")
                            checked: root.installedOnly
                            onToggled: root.installedOnly = checked
                        }
                        SettingsCheckBox {
                            bordered: false
                            text: qsTr("Favorites")
                            checked: root.favoritesOnly
                            onToggled: root.favoritesOnly = checked
                        }
                        SettingsCheckBox {
                            bordered: false
                            text: qsTr("Owned")
                            checked: root.steam.includes("owned")
                            onToggled: root.steam = root.toggled(root.steam, "owned")
                        }
                        SettingsCheckBox {
                            bordered: false
                            text: qsTr("Family")
                            checked: root.steam.includes("family")
                            onToggled: root.steam = root.toggled(root.steam, "family")
                        }
                    }
                    Group {
                        title: qsTr("Status")
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing * 2

                            Repeater {
                                model: Library.statuses

                                SettingsCheckBox {
                                    required property var modelData
                                    bordered: false
                                    text: modelData.name
                                    checked: root.statuses.includes(modelData.id)
                                    onToggled: root.statuses = root.toggled(root.statuses, modelData.id)
                                }
                            }
                        }
                    }
                    Group {
                        title: qsTr("Tags")
                        visible: Library.tags.length > 0
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing * 2

                            Repeater {
                                model: Library.tags

                                SettingsCheckBox {
                                    required property var modelData
                                    bordered: false
                                    text: modelData.name
                                    checked: root.tags.includes(modelData.name)
                                    onToggled: root.tags = root.toggled(root.tags, modelData.name)
                                }
                            }
                        }
                        SettingsCheckBox {
                            visible: root.tags.length >= 2
                            bordered: false
                            text: qsTr("Match all selected tags")
                            checked: root.matchAllTags
                            onToggled: root.matchAllTags = checked
                        }
                    }
                }

                Group {
                    title: root.rulesOn ? qsTr("Added by hand") : qsTr("Games")
                    visible: root.added.length > 0
                    CheckList {
                        Layout.fillWidth: true
                        model: root.gameRows(root.added)
                        ticked: root.kept(root.added, root.dropAdded)
                        maxListHeight: Theme.collections.editListHeight
                        onToggled: id => root.dropAdded = root.toggled(root.dropAdded, id)
                    }
                    Hint { text: qsTr("Uncheck a game to take it out of the collection.") }
                }
                Group {
                    title: qsTr("Left out")
                    visible: root.rulesOn && root.removed.length > 0
                    CheckList {
                        Layout.fillWidth: true
                        model: root.gameRows(root.removed)
                        ticked: root.kept(root.removed, root.dropRemoved)
                        maxListHeight: Theme.collections.editListHeight
                        onToggled: id => root.dropRemoved = root.toggled(root.dropRemoved, id)
                    }
                    Hint { text: qsTr("Uncheck a game to let the rules include it again.") }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            SettingsButton {
                primary: true
                text: root.editing ? qsTr("Save") : root.renaming ? qsTr("Rename") : qsTr("Add")
                enabled: root.editing || root.typed !== ""
                onClicked: root.submit()
            }
        }
    }
}
