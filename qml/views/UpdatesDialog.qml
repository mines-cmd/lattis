import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
UpdateRowsDialog {
    id: root

    // Ids waiting for their turn: one update runs at a time.
    property var queue: []
    property bool pumping: false

    readonly property int pendingCount: rows.filter(r => ["pending", "failed"].includes(statusOf(r.id))).length
    readonly property bool busy: queue.length > 0 || rows.some(r => statusOf(r.id) === "active")
    readonly property bool onlyFailed: pendingCount > 0 && rows.filter(r => statusOf(r.id) === "pending").length === 0

    // The one game this was opened on (its chip, info row or menu entry); empty: every update.
    property string onlyGame: ""

    title: single ? qsTr("Update %1 to %2").arg(rows[0].name).arg(rows[0].version)
         : rows.every(r => r.kind === "game") ? qsTr("Update %1 games").arg(rows.length)
         : rows.every(r => r.kind === "emulator") ? qsTr("Update %1 emulators").arg(rows.length)
         : qsTr("Updates")
    rowIcon: row => Icons.gamepadLine

    function infoOf(row) {
        return row.kind === "game" ? Installer.games[row.gameId] : Installer.emulators[row.id]
    }

    // Opening while updates are still running keeps the rows and the queue as they are.
    function show(gameId) {
        if (!busy) {
            onlyGame = gameId ?? ""
            rows = Installer.updates.filter(u => onlyGame === "" || u.gameId === onlyGame)
                .map(u => ({ id: u.id, kind: u.kind, gameId: u.gameId, name: u.name, version: u.version, from: u.from,
                             size: u.size, notes: u.notes,
                             icon: u.kind === "game" ? (Library.gameWithId(u.gameId).icon ?? "") : "" }))
            queue = []
            openId = rows.length > 0 ? rows[0].id : ""
        }
        open()
    }

    function rowById(id) {
        return rows.find(r => r.id === id)
    }

    function statusOf(id) {
        const info = infoOf(rowById(id) ?? { id })
        if (info && info.state !== "idle" && info.state !== "failed")
            return "active"
        if (queue.includes(id))
            return "waiting"
        if (info && info.state === "failed")
            return "failed"
        return info && info.updateVersion !== "" ? "pending" : "done"
    }

    stateOf: id => {
        const info = infoOf(rowById(id) ?? { id })
        const status = statusOf(id)
        const step = !info ? ""
            : info.state === "checking" ? qsTr("Checking")
            : info.state === "downloading" && info.progress >= 0 ? qsTr("Downloading %1%").arg(Math.round(info.progress * 100))
            : info.state === "downloading" ? qsTr("Downloading")
            : qsTr("Installing")
        return { status, progress: info ? info.progress : -1, step, error: info && status === "failed" ? info.error : "" }
    }
    onUpdateRequested: id => enqueue([id])

    function enqueue(ids) {
        queue = queue.concat(ids.filter(id => !queue.includes(id)))
        pump()
    }

    function updateAll() {
        enqueue(rows.map(r => r.id).filter(id => ["pending", "failed"].includes(statusOf(id))))
    }

    function cancelAll() {
        queue = []
        rows.filter(r => statusOf(r.id) === "active").forEach(r => Installer.cancel(r.id))
    }

    // Installer.update() answers through emulatorsChanged, which lands back here.
    function pump() {
        if (pumping)
            return
        pumping = true
        while (queue.length > 0 && !rows.some(r => statusOf(r.id) === "active")) {
            const id = queue[0]
            queue = queue.slice(1)
            Installer.update(id)
        }
        pumping = false
    }

    Connections {
        target: Installer
        function onEmulatorsChanged() { root.pump() }
    }

    footer: [
        SettingsButton {
            visible: !root.busy && root.pendingCount > 0
            text: qsTr("Later")
            onClicked: root.close()
        },
        SettingsButton {
            visible: root.busy
            text: qsTr("Cancel")
            onClicked: root.cancelAll()
        },
        SettingsButton {
            visible: !root.busy && root.pendingCount > 0
            primary: true
            text: root.onlyFailed ? qsTr("Try again") : root.pendingCount === 1 ? qsTr("Update") : qsTr("Update all")
            onClicked: root.updateAll()
        },
        SettingsButton {
            visible: !root.busy && root.pendingCount === 0
            primary: true
            text: qsTr("Done")
            onClicked: root.close()
        }
    ]
}
