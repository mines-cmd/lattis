import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    readonly property var steam: Library.steam
    // Turning the import off resets what Steam's found row shows, which would flash "wasn't found" as it folds.
    property var held: ({})
    readonly property bool heldLooking: held.enabled && !held.found && !held.problem
    readonly property bool pending: Library.setupPending
    readonly property bool looking: steam.enabled && !steam.found && !steam.problem
    readonly property int notInLibrary: steam.shortcuts - steam.shortcutsInLibrary
    readonly property var tools: steam.presets.filter(p => p.builtIn && (p.program === "" || p.programFound))

    spacing: Theme.onboarding.spacing

    Component.onCompleted: held = steam
    onSteamChanged: if (steam.enabled) held = steam

    function set(key, value) {
        const error = Library.setSteamValue(key, value)
        if (error !== "")
            guide.message(error)
    }

    function setDefault(id, on) {
        const ids = steam.defaultPresets.filter(p => p !== id)
        if (on)
            ids.push(id)
        set("default_presets", steam.presets.map(p => p.id).filter(p => ids.includes(p)))
    }

    component Divider: Rectangle {
        visible: Theme.settingsWindow.rowDividers
        Layout.fillWidth: true
        Layout.leftMargin: -Theme.settingsWindow.rowPadding
        Layout.rightMargin: -Theme.settingsWindow.rowPadding
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
    }

    component Row: RowLayout {
        id: row
        property alias label: name.text
        property alias note: noteLabel.text
        property alias noteColor: noteLabel.color
        default property alias controls: controlRow.data

        Layout.fillWidth: true
        spacing: Theme.layout.spacing * 1.5

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                id: name
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                wrapMode: Text.Wrap
            }
            SettingsNote {
                id: noteLabel
                visible: text !== ""
                Layout.fillWidth: true
            }
        }
        RowLayout {
            id: controlRow
            spacing: Theme.layout.spacing
        }
    }

    component Fold: Unfold {
        required property real gap

        // Out of the layout when folded; the negative margin hides its spacing until it opens, so
        // the gap doesn't snap in at the start of the unfold.
        visible: open || progress > 0
        spacing: Theme.layout.spacing
        Layout.fillWidth: true
        Layout.bottomMargin: -gap * (1 - progress)
    }

    component CardHeader: ColumnLayout {
        id: header
        property string icon
        property alias title: titleLabel.text
        property alias tag: tagLabel.text
        property alias subtitle: subtitleLabel.text

        Layout.fillWidth: true
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Icon {
                path: header.icon
                size: Theme.controls.iconSize
                color: Theme.controls.descriptionColor
            }
            Label {
                id: titleLabel
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                font.weight: Theme.font.strongWeight
            }
            SettingsNote { id: tagLabel }
        }
        SettingsNote {
            id: subtitleLabel
            Layout.fillWidth: true
            Layout.leftMargin: Theme.controls.iconSize + Theme.layout.spacing
        }
    }

    SettingsCard {
        panel: true

        CardHeader {
            icon: Icons.computer
            title: qsTr("Installed on this computer")
            tag: qsTr("No sign-in needed")
            subtitle: qsTr("Games Steam has downloaded here.")
        }
        Fold {
            open: root.steam.enabled
            gap: Theme.layout.spacing
            Divider { visible: Theme.settingsWindow.rowDividers && root.held.enabled }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing * 1.5

                Rectangle {
                    opacity: root.held.found ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    implicitWidth: Theme.onboarding.foundIconSize
                    implicitHeight: Theme.onboarding.foundIconSize
                    radius: width / 2
                    color: Qt.alpha(Theme.onboarding.foundColor, Theme.onboarding.stepDoneOpacity)

                    Icon {
                        anchors.centerIn: parent
                        path: Icons.check
                        size: Math.round(Theme.onboarding.foundIconSize / 2)
                        color: Theme.onboarding.foundColor
                    }
                }
                Row {
                    label: root.heldLooking ? qsTr("Looking for Steam…")
                           : root.held.found ? qsTr("Steam found") : qsTr("Steam wasn't found")
                    note: root.heldLooking ? ""
                          : !root.held.found ? (root.held.path !== "" ? qsTr("There's no Steam in %1.").arg(root.held.path)
                                                 : qsTr("Not in ~/.steam/steam or the Flatpak's folder."))
                          : root.held.problem ? root.held.status
                          : (root.held.flatpak ? qsTr("%1 · Flatpak") : qsTr("%1 · native")).arg(root.held.shownRoot)
                    noteColor: root.held.problem ? Theme.controls.problemsText : Theme.controls.descriptionColor

                    SettingsNote {
                        visible: account.visible
                        text: qsTr("Account")
                    }
                    SteamAccountBox {
                        id: account
                        visible: root.held.found && root.held.users.length > 1
                        Layout.preferredWidth: Theme.settingsWindow.pickerWidth / 3
                        onFailed: message => root.guide.message(message)
                    }
                    Label {
                        visible: root.held.found && root.held.users.length === 1
                        text: root.held.users[0]?.name ?? ""
                        color: Theme.controls.labelColor
                        font.pixelSize: Theme.controls.labelFontSize
                        font.weight: Theme.font.strongWeight
                    }
                    SettingsButton {
                        visible: !root.held.found && !root.heldLooking && root.held.path !== ""
                        text: qsTr("Find automatically")
                        tip: qsTr("Look in ~/.steam/steam, then the Flatpak's folder")
                        onClicked: root.set("path", "")
                    }
                    SettingsButton {
                        visible: !root.held.found && !root.heldLooking
                        text: qsTr("Choose folder")
                        tip: qsTr("Choose Steam's folder: the one with steamapps in it")
                        onClicked: folderDialog.active = true
                    }
                }
            }
            Divider { visible: Theme.settingsWindow.rowDividers && root.held.enabled }
        }

        Row {
            label: qsTr("Import installed games")
            note: !root.held.enabled ? qsTr("Off: installed Steam games won't be imported.")
                  : !root.held.found ? ""
                  : root.held.installed === 0 ? qsTr("No installed games.")
                  : root.pending ? (root.held.installed === 1 ? qsTr("1 installed game will be added when you finish.")
                                    : qsTr("%1 installed games will be added when you finish.").arg(root.held.installed))
                  : root.held.installed === 1 ? qsTr("1 installed game.")
                  : qsTr("%1 installed games.").arg(root.held.installed)

            SettingsSwitch {
                checked: root.held.enabled
                onToggled: root.set("enabled", checked)
            }
        }

        Fold {
            open: root.steam.found
            gap: Theme.layout.spacing
            Divider { visible: Theme.settingsWindow.rowDividers && root.steam.found }
            Row {
                label: qsTr("Non-Steam games")
                note: !root.steam.shortcutsOk ? qsTr("Steam's list of non-Steam games couldn't be read.")
                      : root.steam.shortcuts === 0 ? qsTr("Steam has no non-Steam games.")
                      : root.notInLibrary === 0 ? qsTr("Every non-Steam game in Steam is in the library.")
                      : root.pending && root.guide.shortcutsToAdd.length > 0
                        ? qsTr("%1 of %2 will be added when you finish.").arg(root.guide.shortcutsToAdd.length)
                              .arg(root.notInLibrary)
                      : root.notInLibrary === 1 ? qsTr("1 shortcut in Steam isn't in the library yet.")
                      : qsTr("%1 shortcuts in Steam aren't in the library yet.").arg(root.notInLibrary)

                SettingsButton {
                    text: qsTr("Add existing shortcuts")
                    enabled: root.notInLibrary > 0
                    tip: qsTr("Choose which of Steam's non-Steam games to bring into the library")
                    onClicked: {
                        picker.active = true
                        picker.item.openFresh(root.pending && root.guide.shortcutsToAdd.length > 0
                                              ? root.guide.shortcutsToAdd : undefined)
                    }
                }
            }
        }
    }

    Fold {
        open: root.steam.found && root.steam.enabled
        gap: Theme.onboarding.spacing
        SettingsCard {
            panel: true

            CardHeader {
                icon: Icons.steamLine
                title: qsTr("Your whole library")
                tag: SteamLibrary.authState === "signed_in" ? qsTr("optional") : qsTr("Needs sign-in · optional")
                subtitle: qsTr("Every game on your account, installed or not, including Steam Family shares.")
            }
            Divider { visible: Theme.settingsWindow.rowDividers }
            Row {
                label: SteamLibrary.authState === "signed_in" ? qsTr("Signed in as %1").arg(SteamLibrary.accountName)
                       : qsTr("Sign in with a QR code")
                note: SteamLibrary.authState === "signed_in"
                      ? qsTr("They're added to the library as not installed when you finish. Settings → Steam has the rest.")
                      : qsTr("Scan it with the Steam app. Lattis never sees your password.")

                SettingsButton {
                    visible: SteamLibrary.authState !== "signed_in"
                    text: qsTr("Sign in with Steam")
                    onClicked: {
                        signIn.active = true
                        signIn.item.ask()
                    }
                }
                SettingsButton {
                    visible: SteamLibrary.authState === "signed_in"
                    text: qsTr("Sign out")
                    danger: true
                    onClicked: SteamLibrary.signOut()
                }
            }
        }
    }

    Fold {
        open: root.steam.found && root.tools.length > 0
        gap: Theme.onboarding.spacing
        SettingsCard {
            panel: true

            Row {
                label: qsTr("Start new Steam games with")
                note: qsTr("Added to a game's launch options when it's first imported. Only tools found on this "
                           + "computer are listed.")
            }
            Repeater {
                model: root.tools
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    SettingsCheckBox {
                        text: modelData.name
                        checked: root.steam.defaultPresets.includes(modelData.id)
                        onToggled: root.setDefault(modelData.id, checked)
                    }
                    SettingsNote {
                        Layout.fillWidth: true
                        text: modelData.note
                    }
                }
            }
        }
    }

    SettingsNote {
        visible: root.pending && (root.steam.found || !root.steam.enabled)
        Layout.fillWidth: true
        text: qsTr("Nothing is imported until you finish, so your games come in with the info from "
                   + "“Game info”.")
    }
    // The dialogs' loaders live here, not among the page's children: an empty Loader in the layout
    // still gets a spacing gap, which showed as extra room above the first card.
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Loader {
            id: picker
            active: false
            sourceComponent: ShortcutPicker {
                addNow: !root.pending
                onChosen: appIds => root.guide.shortcutsToAdd = appIds
            }
        }
        Loader {
            id: signIn
            active: false
            sourceComponent: SteamSignInPopup { scrim: true }
        }
        Loader {
            id: folderDialog
            active: false
            sourceComponent: RomFolderDialog {
                onPicked: path => root.set("path", path)
                onClosed: folderDialog.active = false
            }
        }
    }
}
