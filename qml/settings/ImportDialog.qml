import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    readonly property var plan: Backups.importPlan
    readonly property bool library: plan.kind === "library"
    readonly property var parts: plan.parts ?? []
    readonly property bool busy: Backups.importing
    property string done: ""
    property Window hostWindow: null
    readonly property string blocked: (GameRunner.running, Backups.gamesRunning())

    function pick() {
        const hook = Backups.screenshotImport()
        if (hook.file !== undefined) {
            ask(hook.file)
            if (hook.start && error.text === "")
                start()
            return
        }
        fileDialog.currentFolder = Backups.importFolder()
        fileDialog.open()
    }

    function ask(file) {
        done = ""
        error.text = Backups.openImport(file)
        open()
    }

    function start() {
        error.text = Backups.startImport()
    }

    function size(bytes) {
        return bytes < 0 ? qsTr("Adding up…") : Backups.sizeText(bytes)
    }

    function now(target) {
        if (target.problem !== "")
            return ""
        if (!target.exists)
            return qsTr("nothing there now")
        return target.bytes < 0 ? "" : qsTr("%1 now").arg(Backups.sizeText(target.bytes))
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.backupWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, parent.height - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    closePolicy: root.busy || root.done === "library" ? Popup.NoAutoClose : Popup.CloseOnEscape
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
        function onImportFinished(kind, message) {
            if (message === "")
                root.done = kind
            else if (message !== qsTr("Cancelled"))
                error.text = message
        }
    }

    FileDialog {
        id: fileDialog
        parentWindow: root.hostWindow
        title: qsTr("Import")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Backups (*.tar.zst)"), qsTr("All files (*)")]
        onAccepted: root.ask(selectedFile)
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
    component PathHint: BoxHint {
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
        required property var modelData
        readonly property var part: modelData
        readonly property var fine: part.targets.filter(t => t.problem === "")
        readonly property var notFine: part.targets.filter(t => t.problem !== "")

        Layout.fillWidth: true
        spacing: 2
        opacity: part.restorable ? 1 : Theme.gameEditor.readOnlyOpacity

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing
            SettingsCheckBox {
                Layout.fillWidth: true
                text: partRow.part.kind === "besideRoms"
                      ? qsTr("%1, next to %n ROM(s)", "", partRow.part.targets.length).arg(partRow.part.name)
                      : partRow.part.name
                checked: partRow.part.checked
                enabled: partRow.part.restorable && !root.busy
                onToggled: Backups.checkImportPart(partRow.part.index, checked)
            }
            Label {
                visible: partRow.part.kind === "proton" && partRow.part.bytes >= 0
                text: Backups.sizeText(partRow.part.bytes)
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
        }
        BoxHint {
            visible: text !== ""
            text: partRow.part.problem
        }
        BoxHint {
            visible: text !== ""
            text: partRow.part.installText
        }
        Repeater {
            model: partRow.part.problem === "" && (partRow.part.kind === "saves" || partRow.part.kind === "keys") ? partRow.fine : []
            PathHint {
                required property var modelData
                text: modelData.label + (root.now(modelData) === "" ? "" : "  (" + root.now(modelData) + ")")
            }
        }
        BoxHint {
            visible: partRow.part.problem === "" && partRow.part.kind === "proton" && partRow.fine.length > 0
            text: partRow.fine.length > 0 ? (root.now(partRow.fine[0]) === "" ? qsTr("Over its prefix in Steam.")
                                             : qsTr("Over its prefix in Steam (%1).").arg(root.now(partRow.fine[0])))
                                          : ""
        }
        BoxHint {
            visible: partRow.part.problem === "" && partRow.part.kind === "besideRoms" && partRow.fine.length > 0
            text: partRow.fine.map(t => t.label).join(", ")
        }
        Repeater {
            model: partRow.part.problem === "" ? partRow.notFine : []
            BoxHint {
                required property var modelData
                text: qsTr("%1: %2").arg(modelData.label).arg(modelData.problem)
            }
        }
    }
    component PartGroup: ColumnLayout {
        id: group
        property string title
        property var kinds: []
        readonly property var members: root.parts.filter(p => group.kinds.includes(p.kind))

        visible: members.length > 0
        Layout.fillWidth: true
        spacing: Theme.gameEditor.rowSpacing

        Heading { text: group.title }
        Card {
            Repeater {
                model: group.members
                PartRow {}
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            // The plan is gone once done: what was imported names it then.
            readonly property string kind: root.done !== "" ? root.done : (root.plan.kind ?? "")
            text: kind === "library" ? qsTr("Import library") : kind === "saves" ? qsTr("Import saves") : qsTr("Import")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
        }

        Flickable {
            id: flick
            visible: root.plan.kind !== undefined && !root.busy && root.done === ""
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: content.implicitHeight
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - (scrollBar.visible ? scrollBar.width + Theme.layout.spacing : 0)
                spacing: Theme.gameEditor.rowSpacing

                Card {
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing
                        Label {
                            Layout.fillWidth: true
                            text: root.plan.file ?? ""
                            color: Theme.gameEditor.labelColor
                            elide: Text.ElideMiddle
                        }
                        Label {
                            visible: root.library && (root.parts[0]?.bytes ?? -1) >= 0
                            text: Backups.sizeText(root.parts[0]?.bytes ?? 0)
                            color: Theme.gameEditor.hintColor
                            font.pixelSize: Theme.gameEditor.hintFontSize
                        }
                    }
                    Hint {
                        text: root.library && (root.plan.games ?? -1) >= 0
                              ? qsTr("%n game(s), made %1", "", root.plan.games).arg(root.plan.made ?? "")
                              : qsTr("Made %1").arg(root.plan.made ?? "")
                    }
                    Hint {
                        text: root.plan.sameLibrary ? qsTr("From this library.")
                            : root.library ? qsTr("From another library.")
                            : qsTr("From another library: saves of games that aren't in yours stay in the backup. Import its library first to restore them.")
                    }
                    Label {
                        visible: text !== ""
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.gameEditor.warningColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                        text: root.plan.warning ?? ""
                    }
                }

                Heading { visible: root.library; text: qsTr("Replaces your library") }
                Card {
                    visible: root.library
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%n game(s) now", "", root.plan.gamesNow ?? 0)
                            color: Theme.gameEditor.labelColor
                        }
                        Label {
                            text: root.library && root.parts.length > 0 ? root.now(root.parts[0].targets[0]) : ""
                            color: Theme.gameEditor.hintColor
                            font.pixelSize: Theme.gameEditor.hintFontSize
                        }
                    }
                    Hint {
                        text: root.plan.dataDir ?? ""
                        font.family: Theme.font.monoFamily
                        font.pixelSize: Theme.gameEditor.pathFontSize
                        wrapMode: Text.WrapAnywhere
                    }
                    Hint {
                        visible: (root.plan.carriedOver ?? []).length > 0
                        text: qsTr("Kept, as the backup doesn't have them: %1.").arg((root.plan.carriedOver ?? []).join(", "))
                    }
                }

                PartGroup {
                    visible: !root.library && members.length > 0
                    title: qsTr("Emulator saves")
                    kinds: ["saves", "besideRoms"]
                }
                PartGroup {
                    visible: !root.library && members.length > 0
                    title: qsTr("Keys and firmware")
                    kinds: ["keys"]
                }
                PartGroup {
                    visible: !root.library && members.length > 0
                    title: qsTr("Proton games' saves")
                    kinds: ["proton"]
                }
                Hint {
                    visible: !root.library && root.parts.length === 0
                    text: qsTr("This backup holds no saves.")
                }

                Hint {
                    text: root.library ? qsTr("What you have now is moved aside, not deleted: %1. The app restarts when it's done.").arg(root.plan.aside ?? "")
                                       : qsTr("What you have now is moved aside, not deleted, next to where it is.")
                }
            }
        }

        ColumnLayout {
            visible: root.busy
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: Backups.importTotal > 0 ? qsTr("Importing: %1 of %2").arg(Backups.sizeText(Backups.importBytes)).arg(Backups.sizeText(Backups.importTotal))
                                              : qsTr("Importing: %1").arg(Backups.sizeText(Backups.importBytes))
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
                    width: Backups.importTotal > 0 ? parent.width * Math.min(1, Backups.importBytes / Backups.importTotal) : 0
                    color: Theme.gameEditor.progressFill
                }
            }
            Hint { text: qsTr("Nothing changes until all of it is in; Cancel leaves everything as it was.") }
        }

        Label {
            visible: root.done !== "" && !root.busy
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: root.done === "library" ? qsTr("Imported. Restarting…")
                                          : qsTr("Imported. The notice on the main window shows where what you had went.")
            color: Theme.gameEditor.labelColor
        }

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
        }
        Label {
            visible: root.blocked !== "" && root.plan.kind !== undefined && !root.busy && root.done === ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.errorColor
            text: root.blocked
        }
        Hint {
            visible: !root.library && root.plan.kind !== undefined && !root.busy && root.done === ""
            text: qsTr("Close any emulator you started outside the launcher first.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                visible: root.plan.kind !== undefined && !root.busy && root.done === ""
                text: (root.plan.bytes ?? 0) > 0 ? qsTr("%1 to restore").arg(Backups.sizeText(root.plan.bytes)) : ""
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
            Item { Layout.fillWidth: true; visible: !(root.plan.kind !== undefined && !root.busy && root.done === "") }
            SettingsButton {
                visible: root.done !== "library"
                text: root.busy ? qsTr("Cancel") : root.done !== "" || root.plan.kind === undefined ? qsTr("Close") : qsTr("Cancel")
                onClicked: root.busy ? Backups.cancelImport() : root.close()
            }
            SettingsButton {
                visible: root.plan.kind !== undefined && !root.busy && root.done === ""
                primary: true
                text: root.library ? qsTr("Import and restart") : qsTr("Import")
                enabled: root.blocked === "" && !Backups.running && (root.plan.anyChecked ?? false)
                onClicked: root.start()
            }
        }
    }
}
