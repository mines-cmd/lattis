import QtQuick

// See docs/code/qml.md.
LaunchButton {
    id: root

    property var game: ({})
    // False until the first frame is out: the Installer isn't made before.
    property bool live: false

    // An AppImage game's file being replaced; Installer.games has a row only for a game with a release source.
    readonly property bool gameUpdating: live && game.platform === "linux"
        && ["downloading", "extracting", "installing"].includes(Installer.games[game.id ?? ""]?.state ?? "")

    readonly property bool hasGame: game.id !== undefined
    readonly property string launcher: game.launcher ?? ""

    // An emulated game none of whose emulators is found: Launch installs the first one, if it can be.
    // The Installer is only touched then, so it isn't made for a page that doesn't need it.
    readonly property string missingEmulator: {
        void game.launchTip
        void Library.emulators
        return hasGame && game.installed ? GameRunner.missingEmulator(game.id) : ""
    }
    readonly property var emulatorRow: missingEmulator !== "" ? Installer.emulators[missingEmulator] : undefined
    readonly property bool emulatorInstallable: (emulatorRow?.routes?.length ?? 0) > 0
    readonly property string emulatorState: emulatorRow?.state ?? "idle"
    // Tells of a failure that happened here, not one left over from before this game was shown.
    property bool emulatorWasBusy: false

    signal launchRequested()
    signal message(string text)
    signal emulatorMessage(string text, string gameId)

    function openInstallDialog() {
        if (emulatorName === "")
            return
        installDialog.active = true
        installDialog.item.openFor(missingEmulator)
    }

    installed: game.installed ?? true
    updateState: gameUpdating ? "downloading" : (game.steamUpdate ?? "none")
    updatePercent: launcher === "heroic" ? (game.steamUpdatePercent ?? -1) : -1
    updateBlocks: gameUpdating
    rom: game.rom ?? false
    gameNotFound: game.gameNotFound ?? false
    locatable: game.platform === "proton" || game.platform === "linux"
    installable: launcher !== "" && !locatable
    running: GameRunner.running.includes(game.id ?? "")
    emulatorName: emulatorInstallable
        ? (Library.emulators.find(e => e.id === missingEmulator)?.name ?? "") : ""
    emulatorStep: {
        switch (emulatorState) {
        case "checking": return qsTr("Checking")
        case "downloading": return emulatorRow.progress >= 0 ? qsTr("%1 %").arg(Math.round(emulatorRow.progress * 100)) : qsTr("Downloading")
        case "extracting": return qsTr("Unpacking")
        case "installing": return qsTr("Installing")
        default: return ""
        }
    }
    tip: emulatorName !== "" ? qsTr("Install %1; Lattis finds it afterwards").arg(emulatorName)
       : (game.launchTip ?? "") !== "" ? Actions.tip(game.launchTip, Actions.keys.launch) : ""

    onClicked: {
        if (emulatorName === "")
            launchRequested()
        else
            openInstallDialog()
    }

    onEmulatorStepChanged: {
        const busy = emulatorStep !== ""
        if (emulatorWasBusy && !busy && emulatorState === "failed" && !(installDialog.item?.dialog.visible ?? false))
            emulatorMessage(emulatorRow.error, game.id)
        emulatorWasBusy = busy
    }

    Loader {
        id: installDialog
        active: false
        source: "EmulatorInstallDialog.qml"
    }
    Connections {
        target: installDialog.item
        function onError(text) { root.message(text) }
    }
}
