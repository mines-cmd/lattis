import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    readonly property var heroic: Library.heroic
    // Turning the import off empties what Heroic's rows show, which would flash "wasn't found" as they fold.
    property var held: Library.heroic
    // Turning it back on shows that empty map until the scan ends, so the copy waits for the scan.
    property bool settling: false
    readonly property bool pending: Library.setupPending
    readonly property bool notLookedFor: !held.enabled
    // find() only reports a program without a root when it's really Heroic's.
    readonly property bool noLibrary: !held.found && held.enabled && held.programFound
    readonly property bool storesShown: held.found || noLibrary || !heroic.enabled
    // The scan's list, not `games`: with the whole library off, `games` can be 0 for an account that owns plenty.
    readonly property bool empty: held.scanOwned === 0
    readonly property int installed: pending ? held.scanInstalled : held.installed
    readonly property int more: held.scanOwned - held.scanInstalled

    spacing: Theme.onboarding.spacing

    // Ends the binding above: from here on, held only changes through the Connections below.
    Component.onCompleted: held = Library.heroic

    Connections {
        target: Library
        function onHeroicChanged() {
            if (Library.heroic.enabled && !root.settling)
                root.held = Library.heroic
        }
        function onScanningChanged() {
            if (Library.scanning || !root.settling)
                return
            root.settling = false
            root.held = Library.heroic
        }
    }

    function set(key, value) {
        const error = Library.setHeroicValue(key, value)
        if (error !== "")
            guide.message(error)
    }

    function setEnabled(on) {
        settling = on
        set("enabled", on)
    }

    component StoreCard: SettingsCard {
        property alias icon: header.icon
        property alias title: header.title

        panel: true
        enabled: false
        opacity: Theme.gameEditor.readOnlyOpacity

        SetupCardHeader {
            id: header
            tag: qsTr("Not implemented yet")
        }
    }

    SettingsCard {
        panel: true

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing * 1.5

            SetupFoundMark { found: root.held.found || root.noLibrary }
            SetupRow {
                label: root.notLookedFor ? qsTr("Heroic wasn't looked for")
                       : root.held.found || root.noLibrary ? qsTr("Heroic found") : qsTr("Heroic wasn't found")
                note: root.notLookedFor ? qsTr("Turn on Import Epic games to look for it.")
                      : root.noLibrary ? qsTr("It has no Epic library yet.")
                      : !root.held.found ? (root.held.path !== "" ? qsTr("There's no Heroic in %1.").arg(root.held.path)
                                             : qsTr("Not in ~/.config/heroic or the Flatpak's folder."))
                      : !root.held.programFound ? qsTr("Its program wasn't found, so these games can't be launched.")
                      : (root.held.flatpak ? qsTr("%1 · Flatpak") : qsTr("%1 · native")).arg(root.held.shownRoot)
                noteColor: root.held.found && !root.held.programFound ? Theme.controls.problemsText
                                                                      : Theme.controls.descriptionColor

                SettingsButton {
                    visible: !root.held.found && !root.notLookedFor && root.held.path !== ""
                    text: qsTr("Find automatically")
                    tip: qsTr("Look in ~/.config/heroic, then the Flatpak's folder")
                    onClicked: root.set("path", "")
                }
                SettingsButton {
                    visible: !root.held.found && !root.notLookedFor
                    text: qsTr("Choose folder")
                    tip: qsTr("Choose Heroic's folder: the one with store_cache in it")
                    onClicked: folderDialog.active = true
                }
            }
        }
        SettingsNote {
            visible: !root.held.found && !root.notLookedFor && !root.noLibrary
            Layout.fillWidth: true
            linkColor: Theme.gameEditor.linkColor
            text: qsTr("Lattis reads your Epic games from Heroic, which is free. <a href=\"%1\">Get Heroic</a>")
                      .arg("https://heroicgameslauncher.com")
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }

    SetupFold {
        open: root.storesShown
        gap: Theme.onboarding.spacing
        spacing: Theme.onboarding.spacing

        SettingsCard {
            panel: true

            SetupCardHeader {
                icon: Icons.epic
                title: qsTr("Epic Games")
            }
            SetupDivider { visible: Theme.settingsWindow.rowDividers }
            SetupRow {
                label: qsTr("Import Epic games")
                note: !root.heroic.enabled ? qsTr("Off: Epic games won't be imported.")
                      : root.empty ? ""
                      : root.installed === 0 ? qsTr("No installed games.")
                      : root.pending ? (root.installed === 1 ? qsTr("1 installed game will be added when you finish.")
                                        : qsTr("%1 installed games will be added when you finish.").arg(root.installed))
                      : root.installed === 1 ? qsTr("1 installed game.")
                      : qsTr("%1 installed games.").arg(root.installed)

                SettingsSwitch {
                    checked: root.heroic.enabled
                    onToggled: root.setEnabled(checked)
                }
            }
            SetupFold {
                open: root.heroic.enabled && !root.empty
                gap: Theme.layout.spacing
                SetupDivider { visible: Theme.settingsWindow.rowDividers }
                SetupRow {
                    label: qsTr("Import the whole library")
                    note: !root.pending ? (root.heroic.wholeLibrary
                                           ? qsTr("Turning this off takes the games that aren't installed out of the library.")
                                           : qsTr("Turning this on adds the games you own that aren't installed."))
                          : root.more === 0 ? qsTr("You own nothing more than what's installed.")
                          : !root.heroic.wholeLibrary ? qsTr("Off: the %1 more you own won't be added.").arg(root.more)
                          : qsTr("%1 more you own will be added as not installed.").arg(root.more)

                    SettingsSwitch {
                        checked: root.heroic.wholeLibrary
                        onToggled: root.set("whole_library", checked)
                    }
                }
            }
            SetupFold {
                open: root.heroic.enabled && root.empty && !root.settling
                gap: Theme.layout.spacing
                SetupDivider { visible: Theme.settingsWindow.rowDividers }
                SetupRow {
                    label: qsTr("No Epic games yet. Sign in to Epic in Heroic to see them here.")

                    SettingsButton {
                        enabled: root.held.programFound
                        text: qsTr("Open Heroic")
                        onClicked: {
                            const error = GameRunner.openHeroic()
                            if (error !== "")
                                root.guide.message(error)
                        }
                    }
                }
            }
        }
        StoreCard {
            icon: Icons.gog
            title: qsTr("GOG")
        }
        StoreCard {
            icon: Icons.amazon
            title: qsTr("Amazon")
        }
    }

    SettingsNote {
        visible: root.pending && root.storesShown
        Layout.fillWidth: true
        text: qsTr("Nothing is imported until you finish, so your games come in with the info from "
                   + "“Game info”.")
    }
    // The loader lives here, not among the page's children: an empty Loader in the layout still gets
    // a spacing gap, which showed as extra room above the first card.
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

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
