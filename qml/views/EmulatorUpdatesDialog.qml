import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    // What was waiting when the dialog opened, so a row stays after its update is done.
    property var rows: []
    // Ids waiting for their turn: one update runs at a time.
    property var queue: []
    property string openId: ""
    property bool pumping: false

    readonly property bool single: rows.length === 1
    readonly property int pendingCount: rows.filter(r => ["pending", "failed"].includes(statusOf(r.id))).length
    readonly property bool busy: queue.length > 0 || rows.some(r => statusOf(r.id) === "active")
    readonly property bool onlyFailed: pendingCount > 0 && rows.filter(r => statusOf(r.id) === "pending").length === 0

    carded: true
    size: "medium"
    title: single ? qsTr("Update %1 to %2").arg(rows[0].name).arg(rows[0].version)
                  : qsTr("Update %1 emulators").arg(rows.length)
    width: Math.min(Theme.gameEditor.updateWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, Math.round(parent.height * Theme.gameEditor.whatsNewHeightPercent / 100))

    // Opening while updates are still running keeps the rows and the queue as they are.
    function show() {
        if (!busy) {
            rows = Installer.updates.map(u => ({ id: u.id, name: u.name, version: u.version, from: u.from,
                                                  size: u.size, notes: u.notes }))
            queue = []
            openId = rows.length > 0 ? rows[0].id : ""
        }
        open()
    }

    function statusOf(id) {
        const info = Installer.emulators[id]
        if (info && info.state !== "idle" && info.state !== "failed")
            return "active"
        if (queue.includes(id))
            return "waiting"
        if (info && info.state === "failed")
            return "failed"
        return info && info.updateVersion !== "" ? "pending" : "done"
    }

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

    Label {
        visible: root.single
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: !root.single ? ""
            : root.statusOf(root.rows[0].id) === "done" ? qsTr("Updated to %1").arg(root.rows[0].version)
            : sizeVersions(root.rows[0])
    }

    InstallProgress {
        visible: root.single && root.statusOf(root.rows[0].id) === "active"
        Layout.fillWidth: true
        progress: root.single && Installer.emulators[root.rows[0].id] ? Installer.emulators[root.rows[0].id].progress : -1
    }

    function megabytes(bytes) {
        return qsTr("%1 MB").arg(Math.max(1, Math.round(bytes / 1e6)))
    }

    function sizeVersions(row) {
        const versions = row.from !== "" ? qsTr("%1 → %2").arg(row.from).arg(row.version) : row.version
        return row.size > 0 ? qsTr("%1 · %2").arg(versions).arg(megabytes(row.size)) : versions
    }

    Flickable {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: column.implicitHeight
        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: SettingsScrollBar { id: bar }

        // Set from the handlers, not bound: the notes wrap, so their height depends on the width the bar takes.
        // It settles, as a narrower column is only taller.
        property bool scrolls: false
        function recheck() { scrolls = column.implicitHeight > height + 1 }
        onHeightChanged: recheck()

        Connections {
            target: column
            function onImplicitHeightChanged() { view.recheck() }
        }

        ColumnLayout {
            id: column
            width: view.width - (view.scrolls ? bar.reserve : 0)
            spacing: Theme.layout.spacing

            Repeater {
                model: root.rows

                Rectangle {
                    id: card

                    required property var modelData

                    readonly property string status: root.statusOf(modelData.id)
                    readonly property var info: Installer.emulators[modelData.id]
                    readonly property bool expanded: root.single || root.openId === modelData.id

                    Layout.fillWidth: true
                    implicitHeight: content.implicitHeight + 2 * Theme.gameEditor.updateNotesPadding
                    color: Theme.gameEditor.updateNotesBackground
                    radius: Theme.gameEditor.updateNotesRadius
                    clip: true

                    ColumnLayout {
                        id: content
                        anchors.fill: parent
                        anchors.margins: Theme.gameEditor.updateNotesPadding
                        spacing: Theme.layout.spacing

                        RowLayout {
                            id: header
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing
                            visible: !root.single

                            Item {
                                Layout.fillWidth: true
                                implicitHeight: toggle.implicitHeight

                                RowLayout {
                                    id: toggle
                                    anchors.fill: parent
                                    spacing: Theme.layout.spacing

                                    Icon {
                                        path: Icons.chevronRight
                                        size: Theme.controls.iconSize
                                        color: Theme.gameEditor.labelColor
                                        rotation: card.expanded ? 90 : 0
                                        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Label {
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            font.weight: Theme.font.strongWeight
                                            color: Theme.gameEditor.updateNotesText
                                            text: card.modelData.name
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            font.pixelSize: Theme.font.smallSize
                                            color: Theme.gameEditor.labelColor
                                            text: root.sizeVersions(card.modelData)
                                        }
                                    }
                                }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    onTapped: root.openId = card.expanded ? "" : card.modelData.id
                                }
                            }
                            Item {
                                id: state
                                implicitWidth: Math.max(update.implicitWidth, waiting.implicitWidth, progress.implicitWidth,
                                                        done.implicitWidth, retry.implicitWidth)
                                implicitHeight: Math.max(update.implicitHeight, retry.implicitHeight)

                                SettingsButton {
                                    id: update
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Update")
                                    opacity: card.status === "pending" ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                    onClicked: root.enqueue([card.modelData.id])
                                }
                                Label {
                                    id: waiting
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.gameEditor.labelColor
                                    text: qsTr("Waiting")
                                    opacity: card.status === "waiting" ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                }
                                ColumnLayout {
                                    id: progress
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12 * Theme.layout.spacing
                                    spacing: Theme.layout.spacingSmall
                                    opacity: card.status === "active" ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                    Label {
                                        Layout.alignment: Qt.AlignRight
                                        font.pixelSize: Theme.font.smallSize
                                        color: Theme.gameEditor.labelColor
                                        text: !card.info ? ""
                                            : card.info.state === "checking" ? qsTr("Checking")
                                            : card.info.state === "downloading" && card.info.progress >= 0
                                                ? qsTr("Downloading %1%").arg(Math.round(card.info.progress * 100))
                                            : card.info.state === "downloading" ? qsTr("Downloading")
                                            : qsTr("Installing")
                                    }
                                    InstallProgress {
                                        Layout.fillWidth: true
                                        progress: card.info ? card.info.progress : -1
                                    }
                                }
                                RowLayout {
                                    id: done
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.layout.spacingSmall
                                    opacity: card.status === "done" ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                    Icon {
                                        path: Icons.check
                                        size: Theme.controls.iconSize
                                        color: Theme.settingsWindow.installProgressFill
                                    }
                                    Label {
                                        color: Theme.gameEditor.labelColor
                                        text: qsTr("Updated")
                                    }
                                }
                                SettingsButton {
                                    id: retry
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Try again")
                                    opacity: card.status === "failed" ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                    onClicked: root.enqueue([card.modelData.id])
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: card.status === "failed" && !!card.info && card.info.error !== ""
                            wrapMode: Text.WordWrap
                            color: Theme.gameEditor.errorColor
                            text: card.info ? card.info.error : ""
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: card.expanded ? notes.implicitHeight : 0
                            opacity: card.expanded ? 1 : 0
                            visible: opacity > 0
                            clip: true
                            Behavior on Layout.preferredHeight { enabled: Motion.on; MotionNumber {} }
                            Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                            Label {
                                id: notes
                                width: parent.width
                                wrapMode: Text.WordWrap
                                textFormat: Text.RichText
                                color: Theme.gameEditor.updateNotesText
                                text: AppUpdate.notesHtml(card.modelData.notes !== "" ? card.modelData.notes : qsTr("No release notes."),
                                                          Qt.alpha(Theme.gameEditor.updateNotesText, 0.25))
                                onLinkActivated: link => Qt.openUrlExternally(link)
                            }
                        }
                    }
                }
            }
        }
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
