import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string kind: "library"
    readonly property bool library: kind === "library"
    readonly property var plan: library ? Backups.libraryPlan : Backups.savesPlan
    readonly property var choices: Backups.choices
    readonly property bool busy: Backups.running && Backups.kind === kind
    property var done: null
    // A game the launcher started is running: the saves wait (re-read as games start and stop).
    readonly property string blocked: (GameRunner.running, library ? "" : Backups.gamesRunning())

    function ask(k) {
        kind = k
        done = null
        error.text = ""
        if (k === "library")
            Backups.planLibrary()
        else
            Backups.planSaves()
        open()
        startWhenMeasured = Backups.screenshotTarget().toString() !== ""
    }

    property bool startWhenMeasured: false
    onPlanChanged: if (startWhenMeasured && (plan.measured ?? false)) {
        startWhenMeasured = false
        start(Backups.screenshotTarget())
    }

    function start(file) {
        error.text = Backups.start(kind, file)
    }

    function size(bytes) {
        return bytes < 0 ? qsTr("Adding up…") : Backups.sizeText(bytes)
    }

    readonly property real totalBytes: {
        const p = plan
        if (!(p.measured ?? false))
            return -1
        if (library)
            return p.dataBytes + (choices.credentials && p.credentials.exists ? p.credentials.bytes : 0)
        let total = 0
        for (const e of p.emulators)
            total += Math.max(0, e.savesBytes) + Math.max(0, e.besideBytes) + (choices.keys ? Math.max(0, e.keysBytes) : 0)
        if (choices.proton)
            for (const g of p.proton)
                total += g.skipped === "" ? Math.max(0, g.bytes) : 0
        return total
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.backupWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    closePolicy: root.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    Overlay.modal: Scrim {}
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    Connections {
        target: Backups
        function onFinished(kind, path, message) {
            if (kind !== root.kind)
                return
            if (message === "")
                root.done = Backups.last[kind]
            else if (message !== qsTr("Cancelled"))
                error.text = message
        }
    }

    FileDialog {
        id: saveDialog
        title: root.library ? qsTr("Back up library") : qsTr("Back up saves")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Backups (*.tar.zst)")]
        defaultSuffix: "tar.zst"
        onAccepted: root.start(selectedFile)
    }

    component Hint: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }
    component BoxHint: Hint {
        leftPadding: Theme.controls.checkboxSize + Theme.layout.spacing
    }
    component PathHint: Hint {
        font.family: Theme.font.monoFamily
        font.pixelSize: Theme.gameEditor.pathFontSize
        wrapMode: Text.WrapAnywhere
    }
    component Heading: Label {
        Layout.fillWidth: true
        color: Theme.gameEditor.labelColor
        font.weight: Theme.font.strongWeight
        wrapMode: Text.WordWrap
    }
    component Card: Surface {
        default property alias fields: cardColumn.data

        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + 2 * Theme.gameEditor.cardPadding
        radius: Theme.gameEditor.cardRadius
        fill: Theme.gameEditor.cardBackground
        fillEnd: Theme.gameEditor.cardBackgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.cardBorder
        border.width: Theme.gameEditor.cardBorderWidth

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Theme.gameEditor.cardPadding
            spacing: Theme.gameEditor.rowSpacing
        }
    }
    component PartRow: ColumnLayout {
        id: partRow
        property string name
        property string detail
        property bool skipped: false
        default property alias notes: notesColumn.data

        Layout.fillWidth: true
        spacing: 2
        opacity: skipped ? Theme.gameEditor.readOnlyOpacity : 1

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing
            Label {
                Layout.fillWidth: true
                text: partRow.name
                color: Theme.gameEditor.labelColor
                elide: Text.ElideRight
            }
            Label {
                text: partRow.detail
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
        }
        ColumnLayout {
            id: notesColumn
            Layout.fillWidth: true
            spacing: 2
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: root.library ? qsTr("Back up library") : qsTr("Back up saves")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
        }

        Flickable {
            id: flick
            visible: !root.busy && root.done === null
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: parts.implicitHeight
            contentHeight: parts.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: parts
                width: flick.width - (scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0)
                spacing: Theme.gameEditor.rowSpacing

                Card {
                    visible: root.library

                    PartRow {
                        name: qsTr("Library and settings")
                        detail: root.size(root.plan.dataBytes ?? -1)
                        PathHint { text: root.plan.dataDir ?? "" }
                        Hint {
                            text: qsTr("%n game(s), with their art, play time and tags; themes, platforms, emulators and settings.",
                                       "", root.plan.games ?? 0)
                        }
                    }
                    ColumnLayout {
                        visible: root.plan.credentials?.exists ?? false
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            SettingsCheckBox {
                                Layout.fillWidth: true
                                text: qsTr("API keys (IGDB, SteamGridDB)")
                                checked: root.choices.credentials
                                onToggled: error.text = Backups.setChoice("credentials", checked)
                            }
                            Label {
                                text: root.size(root.plan.credentials?.bytes ?? -1)
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                            }
                        }
                        BoxHint { text: qsTr("credentials.toml. Anyone with the file can use them.") }
                    }
                    Repeater {
                        model: root.library ? (root.plan.roms ?? []) : []
                        Hint {
                            required property var modelData
                            text: qsTr("ROMs in %1 aren't included (%2).").arg(modelData.path).arg(root.size(modelData.bytes))
                        }
                    }
                }

                Heading { visible: !root.library; text: qsTr("Emulator saves") }
                Card {
                    visible: !root.library
                    Hint {
                        visible: (root.plan.emulators ?? []).length === 0
                        text: qsTr("No platform lists an emulator, and no game picks one.")
                    }
                    Repeater {
                        model: root.library ? [] : (root.plan.emulators ?? [])
                        PartRow {
                            required property var modelData
                            readonly property bool nothing: modelData.saves.length === 0 && modelData.besideRoms === 0
                            name: modelData.name
                            skipped: nothing
                            detail: modelData.skipped === "notInstalled" && nothing ? qsTr("Not installed")
                                  : modelData.skipped === "noSaves" ? qsTr("Doesn't say where it keeps its saves")
                                  : nothing && (root.plan.measured ?? false) ? qsTr("No saves yet")
                                  : root.size(modelData.savesBytes < 0 || modelData.besideBytes < 0 ? -1
                                              : modelData.savesBytes + modelData.besideBytes)
                            Hint {
                                visible: text !== ""
                                text: modelData.describe !== qsTr("Not found") ? modelData.describe : ""
                            }
                            Repeater {
                                model: modelData.saves
                                PathHint { required property string modelData; text: modelData }
                            }
                            Hint {
                                visible: modelData.besideRoms > 0
                                text: qsTr("Saves next to %n ROM(s)", "", modelData.besideRoms)
                            }
                            Hint {
                                visible: modelData.skipped === "noSaves"
                                text: qsTr("Set its saves in emulators.toml to back them up.")
                            }
                        }
                    }
                }

                RowLayout {
                    visible: !root.library
                    Layout.fillWidth: true
                    SettingsCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Keys and firmware")
                        checked: root.choices.keys
                        onToggled: error.text = Backups.setChoice("keys", checked)
                    }
                }
                Hint {
                    visible: !root.library
                    text: qsTr("They're your console's; don't share this file.")
                }
                Card {
                    id: keysCard
                    readonly property var withKeys: (root.plan.emulators ?? []).filter(e => e.keys.length > 0)
                    visible: !root.library && withKeys.length > 0
                    enabled: root.choices.keys
                    opacity: enabled ? 1 : Theme.gameEditor.readOnlyOpacity
                    Repeater {
                        model: keysCard.withKeys
                        PartRow {
                            required property var modelData
                            name: modelData.name
                            detail: root.size(modelData.keysBytes)
                            Repeater {
                                model: modelData.keys
                                PathHint { required property string modelData; text: modelData }
                            }
                        }
                    }
                }

                RowLayout {
                    visible: !root.library
                    Layout.fillWidth: true
                    SettingsCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Proton games' saves")
                        checked: root.choices.proton
                        onToggled: error.text = Backups.setChoice("proton", checked)
                    }
                }
                Hint {
                    visible: !root.library
                    text: qsTr("What each game keeps in its Wine prefix, without the files Proton put there.")
                }
                Card {
                    visible: !root.library && (root.plan.proton ?? []).length > 0
                    enabled: root.choices.proton
                    opacity: enabled ? 1 : Theme.gameEditor.readOnlyOpacity
                    Repeater {
                        model: root.library ? [] : (root.plan.proton ?? [])
                        PartRow {
                            required property var modelData
                            name: modelData.title
                            skipped: modelData.skipped !== ""
                            detail: modelData.skipped === "neverLaunched" ? qsTr("Never launched")
                                  : modelData.skipped === "uninstalled" ? qsTr("Not installed")
                                  : modelData.skipped === "unlisted" ? qsTr("Its Proton doesn't list its files")
                                  : root.size(modelData.bytes)
                        }
                    }
                }

                Heading { text: qsTr("Not in this backup") }
                Hint {
                    text: (root.library ? qsTr("Saves: they're in their own backup.") + "\n" : qsTr("The library: it's in its own backup.") + "\n")
                          + qsTr("ROMs.") + "\n"
                          + qsTr("Steam games (Steam Cloud and Steam keep those), and their Proton prefixes.") + "\n"
                          + qsTr("Linux games' saves, wherever each game keeps them.") + "\n"
                          + qsTr("The cache, window sizes and the Steam sync record.")
                    opacity: Theme.gameEditor.readOnlyOpacity
                }
                SettingsButton {
                    text: root.library ? qsTr("Back up saves") : qsTr("Back up library")
                    onClicked: root.ask(root.library ? "saves" : "library")
                }
            }
        }

        ColumnLayout {
            visible: root.busy
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: Backups.total > 0 ? qsTr("Backing up: %1 of %2").arg(Backups.sizeText(Backups.bytes)).arg(Backups.sizeText(Backups.total))
                                        : qsTr("Backing up: %1").arg(Backups.sizeText(Backups.bytes))
                color: Theme.gameEditor.labelColor
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Theme.gameEditor.progressHeight
                radius: Theme.gameEditor.progressRadius
                color: Theme.gameEditor.progressTrack
                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    width: Backups.total > 0 ? parent.width * Math.min(1, Backups.bytes / Backups.total) : 0
                    color: Theme.gameEditor.progressFill
                }
            }
            Hint { text: Backups.part }
            Hint { text: qsTr("You can keep using the app meanwhile.") }
        }

        ColumnLayout {
            visible: root.done !== null && !root.busy
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Backed up.")
                color: Theme.gameEditor.labelColor
            }
            PathHint { text: root.done?.path ?? "" }
        }

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
        }
        Label {
            visible: root.blocked !== "" && !root.busy && root.done === null
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
            text: root.blocked
        }
        Hint {
            visible: !root.library && !root.busy && root.done === null
            text: qsTr("Close any emulator you started outside the launcher first.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                visible: !root.busy && root.done === null
                text: root.totalBytes < 0 ? qsTr("Adding up…")
                                          : qsTr("%1 before compression").arg(Backups.sizeText(root.totalBytes))
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
            Item { Layout.fillWidth: true; visible: root.busy || root.done !== null }
            SettingsButton {
                visible: root.done !== null && !root.busy
                text: qsTr("Show")
                onClicked: Qt.openUrlExternally(root.done.folder)
            }
            SettingsButton {
                text: root.busy ? qsTr("Cancel") : root.done !== null ? qsTr("Close") : qsTr("Cancel")
                onClicked: root.busy ? Backups.cancel() : root.close()
            }
            SettingsButton {
                visible: !root.busy && root.done === null
                primary: true
                text: qsTr("Back up")
                enabled: root.blocked === "" && !Backups.running
                onClicked: {
                    const file = Backups.defaultFile(root.kind)
                    saveDialog.currentFolder = file.toString().substring(0, file.toString().lastIndexOf("/"))
                    saveDialog.selectedFile = file
                    saveDialog.open()
                }
            }
        }
    }
}
