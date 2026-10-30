import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string editId: ""
    property var linkable: []
    property var checked: ({})
    property var tools: []
    property string toolDefault: ""

    readonly property bool editing: editId !== ""
    readonly property string exe: exeField.text.trim()
    readonly property bool proton: exe.toLowerCase().endsWith(".exe")
    readonly property var linked: linkable.filter(g => checked[g.id])
    readonly property var prefixGames: linked.filter(g => g.platform === "steam" || g.platform === "proton")
    readonly property bool canChoosePrefix: proton && prefixGames.length === 1
    readonly property bool sharesPrefix: canChoosePrefix && prefixChoice.currentIndex === 0
    readonly property bool canSave: nameField.text.trim() !== "" && exe !== ""
    readonly property string query: gameSearch.text.trim().toLowerCase()
    readonly property var shownGames: linkable.filter(g => query === "" || g.title.toLowerCase().includes(query))

    signal saved(string name)

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
        gameSearch.clear()
        error.text = ""
        open()
        nameField.forceActiveFocus()
    }

    function save() {
        if (!canSave)
            return
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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.settingsWindow.pickerWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.gameEditor.padding)
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

    component FieldLabel: Label {
        color: Theme.gameEditor.labelColor
    }
    component Hint: Label {
        Layout.fillWidth: true
        color: Theme.controls.descriptionColor
        font.pixelSize: Theme.controls.descriptionFontSize
        wrapMode: Text.WordWrap
    }

    contentItem: Flickable {
        id: flick
        clip: true
        contentHeight: form.implicitHeight
        implicitHeight: form.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar {}

        ColumnLayout {
            id: form
            width: flick.width - 12
            spacing: Theme.gameEditor.rowSpacing

            Label {
                text: root.editing ? qsTr("Edit mod manager") : qsTr("Add mod manager")
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
            }

            FieldLabel { text: qsTr("Name") }
            SettingsTextField {
                id: nameField
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
                }
                SettingsButton {
                    text: qsTr("Browse")
                    onClicked: programDialog.active = true
                    Loader {
                        id: programDialog
                        active: false
                        sourceComponent: ProgramDialog {
                            title: qsTr("Choose the mod manager's program")
                            onPicked: path => exeField.text = path
                            onClosed: programDialog.active = false
                        }
                    }
                }
            }
            Hint {
                visible: root.exe !== ""
                text: root.proton ? qsTr("A Windows program: it runs through Proton as a Steam shortcut.")
                                  : qsTr("It runs directly, like a Linux game.")
            }

            FieldLabel { text: qsTr("Games it mods") }
            SettingsSearchField {
                id: gameSearch
                Layout.fillWidth: true
                placeholderText: qsTr("Search games")
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(gameList.contentHeight, 5 * Theme.controls.inputHeight) + 2
                radius: Theme.controls.inputRadius
                color: Theme.controls.inputBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth

                ListView {
                    id: gameList
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.shownGames
                    ScrollBar.vertical: SettingsScrollBar {}

                    delegate: RowLayout {
                        id: gameRow
                        required property var modelData
                        width: ListView.view.width
                        spacing: Theme.layout.spacing

                        SettingsCheckBox {
                            checked: root.checked[gameRow.modelData.id] ?? false
                            onToggled: {
                                const c = Object.assign({}, root.checked)
                                c[gameRow.modelData.id] = checked
                                root.checked = c
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: gameRow.modelData.title
                            color: Theme.controls.labelColor
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.rightMargin: 10
                            text: gameRow.modelData.platformName
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.settingsWindow.markerFontSize
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: root.shownGames.length === 0
                    text: root.linkable.length === 0 ? qsTr("No Steam, Proton or Linux games yet") : qsTr("No games match")
                    color: Theme.controls.descriptionColor
                }
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

            FieldLabel { visible: root.proton && !root.sharesPrefix; text: qsTr("Proton version") }
            SettingsComboBox {
                id: protonChoice
                visible: root.proton && !root.sharesPrefix
                Layout.fillWidth: true
                model: [root.toolDefault !== "" ? qsTr("Steam's default (%1)").arg(root.toolDefault) : qsTr("Steam's default")]
                    .concat(root.tools.map(t => t.displayName))
            }

            FieldLabel { text: qsTr("Launch options") }
            SettingsTextField {
                id: optionsField
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                placeholderText: qsTr("Arguments, or VAR=value %command% for a Proton program")
                font.family: Theme.font.monoFamily
            }

            FieldLabel { text: qsTr("Start folder") }
            SettingsTextField {
                id: startField
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

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing
                Item { Layout.fillWidth: true }
                SettingsButton { text: qsTr("Cancel"); onClicked: root.close() }
                SettingsButton {
                    primary: true
                    text: root.editing ? qsTr("Save") : qsTr("Add")
                    enabled: root.canSave
                    onClicked: root.save()
                }
            }
        }
    }
}
