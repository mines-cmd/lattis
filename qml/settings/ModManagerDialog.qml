import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    carded: true
    size: "medium"
    title: editing ? qsTr("Edit mod manager") : qsTr("Add mod manager")

    property string editId: ""
    property var linkable: []
    property var checked: ({})
    property var tools: []
    property string toolDefault: ""
    property var shortcut: null

    readonly property bool editing: editId !== ""
    readonly property bool adopting: !editing && shortcut !== null
    readonly property string exe: exeField.text.trim()
    readonly property bool proton: exe.toLowerCase().endsWith(".exe")
    readonly property var linked: linkable.filter(g => checked[g.id])
    readonly property var prefixGames: linked.filter(g => g.platform === "steam" || g.platform === "proton")
    readonly property bool canChoosePrefix: proton && !adopting && prefixGames.length === 1
    readonly property bool sharesPrefix: canChoosePrefix && prefixChoice.currentIndex === 0
    readonly property bool canSave: nameField.text.trim() !== "" && exe !== ""

    signal saved(string name)

    function setLinked(id, on) {
        const c = Object.assign({}, checked)
        c[id] = on
        checked = c
    }

    function setLinkedMany(ids, on) {
        const c = Object.assign({}, checked)
        for (const id of ids)
            c[id] = on
        checked = c
    }

    function useShortcut(s) {
        shortcut = s
        nameField.text = s.name
        exeField.text = s.exe
    }

    function openFor(manager, gameIds) {
        const choices = Library.modManagerChoices()
        tools = choices.protonTools
        toolDefault = choices.protonDefault
        linkable = Library.linkableGames()
        editId = manager ? manager.id : ""
        nameField.text = manager ? manager.name : ""
        exeField.text = manager ? manager.exe : ""
        startField.text = manager ? manager.startDir : ""
        optionsField.text = manager ? manager.launchOptions : ""
        const c = {}
        for (const g of manager ? manager.games : (gameIds ?? []))
            c[typeof g === "string" ? g : g.id] = true
        checked = c
        prefixChoice.currentIndex = manager && manager.prefix === "own" ? 1 : 0
        const i = tools.findIndex(t => t.name === (manager?.protonTool ?? ""))
        protonChoice.currentIndex = i < 0 ? 0 : i + 1
        shortcut = null
        gameList.clearSearch()
        error.text = ""
        open()
        nameField.forceActiveFocus()
    }

    function save() {
        if (!canSave)
            return
        if (adopting) {
            const ids = linked.map(g => g.id)
            error.text = Library.addSteamShortcutAsModManager(shortcut.appId, ids)
            if (error.text === "") {
                close()
                saved(shortcut.name)
            }
            return
        }
        const values = {
            name: nameField.text.trim(),
            exe: exe,
            start_dir: startField.text.trim(),
            launch_options: optionsField.text.trim(),
            games: Object.keys(checked).filter(id => checked[id] && linkable.some(g => g.id === id)),
            prefix: canChoosePrefix ? (prefixChoice.currentIndex === 0 ? "game" : "own") : "",
            proton: proton && !sharesPrefix && protonChoice.currentIndex > 0
                    ? tools[protonChoice.currentIndex - 1].name : ""
        }
        if (editing) {
            error.text = Library.setModManagerValues(editId, values)
        } else {
            for (const key of Object.keys(values)) {
                if (values[key] === "")
                    delete values[key]
            }
            error.text = Library.addModManager(values).error ?? ""
        }
        if (error.text === "") {
            close()
            saved(values.name)
        }
    }

    width: Math.min(Theme.settingsWindow.pickerWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.gameEditor.padding)

    component FieldLabel: Label {
        color: Theme.gameEditor.labelColor
    }
    component Hint: Label {
        Layout.fillWidth: true
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }

    Flickable {
        id: flick
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: form.implicitHeight
        clip: true
        contentHeight: form.implicitHeight
        implicitHeight: form.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar {}

        ColumnLayout {
            id: form
            width: flick.width - 12
            spacing: Theme.gameEditor.rowSpacing

            FieldLabel { text: qsTr("Name") }
            SettingsTextField {
                id: nameField
                enabled: !root.adopting
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                placeholderText: qsTr("e.g. Mod Organizer 2")
                onAccepted: root.save()
            }

            FieldLabel { text: qsTr("Program") }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing
                SettingsTextField {
                    id: exeField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    placeholderText: qsTr("A .exe, a script, an AppImage or a Flatpak app id")
                    onTextEdited: root.shortcut = null
                }
                SettingsButton {
                    text: qsTr("Browse")
                    onClicked: programDialog.active = true
                    Loader {
                        id: programDialog
                        active: false
                        sourceComponent: ProgramDialog {
                            title: qsTr("Choose the mod manager's program")
                            onPicked: path => {
                                root.shortcut = null
                                exeField.text = path
                            }
                            onClosed: programDialog.active = false
                        }
                    }
                }
                SettingsButton {
                    visible: !root.editing
                    text: qsTr("From Steam")
                    tip: qsTr("Use a non-Steam game Steam already has")
                    onClicked: {
                        shortcutPicker.active = true
                        shortcutPicker.item.openFresh()
                    }
                    Loader {
                        id: shortcutPicker
                        active: false
                        sourceComponent: ShortcutPicker {
                            choosing: true
                            onChoice: s => root.useShortcut(s)
                        }
                    }
                }
            }
            Hint {
                visible: root.exe !== ""
                text: root.adopting ? qsTr("It keeps its Steam shortcut: launch options, Proton version and Wine prefix stay as they are.")
                      : root.proton ? qsTr("A Windows program: it runs through Proton as a Steam shortcut.")
                                    : qsTr("It runs directly, like a Linux game.")
            }

            CheckList {
                id: gameList
                Layout.fillWidth: true
                label: qsTr("Games it mods")
                noun: qsTr("%n game(s)", "", root.linkable.length)
                withSelectAll: true
                emptyText: qsTr("No Steam, Proton or Linux games yet")
                maxListHeight: 5 * Theme.collections.checklistRowHeight
                model: root.linkable.map(g => ({ id: g.id, title: g.title, info: g.platformName }))
                ticked: root.linkable.filter(g => root.checked[g.id]).map(g => g.id)
                onToggled: id => root.setLinked(id, !root.checked[id])
                onToggledMany: (ids, on) => root.setLinkedMany(ids, on)
            }
            Hint { text: qsTr("Steam, Proton and Linux games can be linked. ROM and emulator games can't.") }

            FieldLabel { visible: root.canChoosePrefix; text: qsTr("Prefix") }
            SettingsComboBox {
                id: prefixChoice
                visible: root.canChoosePrefix
                Layout.fillWidth: true
                model: [qsTr("The game's prefix"), qsTr("Its own prefix")]
            }
            Hint {
                visible: root.canChoosePrefix
                text: root.sharesPrefix
                      ? qsTr("It runs in %1's Wine prefix, so it sees the game's registry and saves. Its Proton version follows the game's.")
                            .arg(root.prefixGames[0].title)
                      : qsTr("It gets a Wine prefix of its own.")
            }

            FieldLabel { visible: root.proton && !root.adopting && !root.sharesPrefix; text: qsTr("Proton version") }
            SettingsComboBox {
                id: protonChoice
                visible: root.proton && !root.adopting && !root.sharesPrefix
                Layout.fillWidth: true
                model: [root.toolDefault !== "" ? qsTr("Steam's default (%1)").arg(root.toolDefault) : qsTr("Steam's default")]
                    .concat(root.tools.map(t => t.displayName))
            }

            FieldLabel { visible: !root.adopting; text: qsTr("Launch options") }
            SettingsTextField {
                id: optionsField
                visible: !root.adopting
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                placeholderText: qsTr("Arguments, or VAR=value %command% for a Proton program")
                font.family: Theme.font.monoFamily
            }

            FieldLabel { visible: !root.adopting; text: qsTr("Start folder") }
            SettingsTextField {
                id: startField
                visible: !root.adopting
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                placeholderText: qsTr("The program's folder")
            }

            Label {
                id: error
                visible: text !== ""
                Layout.fillWidth: true
                color: Theme.controls.problemsText
                wrapMode: Text.WordWrap
            }
        }
    }

    footer: [
        SettingsButton { text: qsTr("Cancel"); onClicked: root.close() },
        SettingsButton {
            primary: true
            text: root.editing ? qsTr("Save") : qsTr("Add")
            enabled: root.canSave
            onClicked: root.save()
        }
    ]
}
