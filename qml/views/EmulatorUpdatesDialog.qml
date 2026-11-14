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
        Layout.preferredHeight: card.implicitHeight
        contentWidth: width
        contentHeight: card.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: SettingsScrollBar { id: bar }

        // Set from the handlers, not bound: the notes wrap, so their height depends on the width the bar takes.
        // It settles, as a narrower card is only taller.
        property bool scrolls: false
        function recheck() { scrolls = card.implicitHeight > height + 1 }
        onHeightChanged: recheck()

        Connections {
            target: card
            function onImplicitHeightChanged() { view.recheck() }
        }

        // The Platforms page's list: one card, its rows told apart by lines, the open one on a darker panel.
        Rectangle {
            id: card
            width: view.width - (view.scrolls ? bar.reserve : 0)
            implicitHeight: cardColumn.implicitHeight + Theme.settingsWindow.cardPadding * 2
            radius: Theme.settingsWindow.cardRadius
            color: Theme.settingsWindow.cardBackground
            border.color: Theme.settingsWindow.cardBorder
            border.width: Theme.settingsWindow.cardBorderWidth

            ColumnLayout {
                id: cardColumn
                anchors.fill: parent
                anchors.margins: Theme.settingsWindow.cardPadding
                spacing: 0

                Repeater {
                    model: root.rows

                    ColumnLayout {
                        id: row

                        required property var modelData
                        required property int index

                        readonly property string status: root.statusOf(modelData.id)
                        readonly property var info: Installer.emulators[modelData.id]
                        readonly property bool expanded: root.single || root.openId === modelData.id
                        readonly property bool firstInCard: index === 0
                        readonly property bool lastInCard: index === root.rows.length - 1
                        readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                                          Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)

                        Layout.fillWidth: true
                        spacing: 0

                        Rectangle {
                            id: header
                            visible: !root.single
                            Layout.fillWidth: true
                            implicitHeight: headerRow.implicitHeight + Theme.settingsWindow.rowPadding * 2
                            radius: Theme.settingsWindow.rowRadius
                            topLeftRadius: row.firstInCard ? row.cardCornerRadius : radius
                            topRightRadius: topLeftRadius
                            bottomLeftRadius: details.progress > 0 ? 0 : row.lastInCard ? row.cardCornerRadius : radius
                            bottomRightRadius: bottomLeftRadius
                            readonly property bool lit: headerHover.hovered || header.activeFocus
                            color: lit && details.progress === 0 ? Theme.controls.hover : "transparent"
                            activeFocusOnTab: true

                            function toggle() { root.openId = row.expanded ? "" : row.modelData.id }

                            Keys.onReturnPressed: toggle()
                            Keys.onEnterPressed: toggle()
                            Keys.onSpacePressed: toggle()

                            HoverHandler { id: headerHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: { header.forceActiveFocus(); header.toggle() }
                            }

                            OpenRowCard {
                                row: row
                                progress: details.progress
                                firstInCard: row.firstInCard
                                lastInCard: row.lastInCard
                                lit: header.lit
                            }

                            RowLayout {
                                id: headerRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Theme.settingsWindow.rowPadding
                                spacing: Theme.layout.spacing

                                Icon {
                                    path: Icons.chevronRight
                                    size: Theme.controls.iconSize
                                    color: Theme.controls.iconColor
                                    rotation: row.expanded ? 90 : 0
                                    Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        color: Theme.controls.labelColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: root.sizeVersions(row.modelData)
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                        elide: Text.ElideRight
                                    }
                                }
                                Item {
                                    implicitWidth: Math.max(update.implicitWidth, waiting.implicitWidth, progress.implicitWidth,
                                                            done.implicitWidth, retry.implicitWidth)
                                    implicitHeight: Math.max(update.implicitHeight, retry.implicitHeight)

                                    SettingsButton {
                                        id: update
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Update")
                                        opacity: row.status === "pending" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                        onClicked: root.enqueue([row.modelData.id])
                                    }
                                    Label {
                                        id: waiting
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                        text: qsTr("Waiting")
                                        opacity: row.status === "waiting" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                    }
                                    ColumnLayout {
                                        id: progress
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 12 * Theme.layout.spacing
                                        spacing: Theme.layout.spacingSmall
                                        opacity: row.status === "active" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                        Label {
                                            Layout.alignment: Qt.AlignRight
                                            color: Theme.controls.descriptionColor
                                            font.pixelSize: Theme.controls.descriptionFontSize
                                            text: !row.info ? ""
                                                : row.info.state === "checking" ? qsTr("Checking")
                                                : row.info.state === "downloading" && row.info.progress >= 0
                                                    ? qsTr("Downloading %1%").arg(Math.round(row.info.progress * 100))
                                                : row.info.state === "downloading" ? qsTr("Downloading")
                                                : qsTr("Installing")
                                        }
                                        InstallProgress {
                                            Layout.fillWidth: true
                                            progress: row.info ? row.info.progress : -1
                                        }
                                    }
                                    RowLayout {
                                        id: done
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: Theme.layout.spacingSmall
                                        opacity: row.status === "done" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                        Icon {
                                            path: Icons.check
                                            size: Theme.controls.iconSize
                                            color: Theme.settingsWindow.installProgressFill
                                        }
                                        Label {
                                            color: Theme.controls.descriptionColor
                                            font.pixelSize: Theme.controls.descriptionFontSize
                                            text: qsTr("Updated")
                                        }
                                    }
                                    SettingsButton {
                                        id: retry
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Try again")
                                        opacity: row.status === "failed" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                        onClicked: root.enqueue([row.modelData.id])
                                    }
                                }
                            }
                        }

                        Label {
                            visible: row.status === "failed" && !!row.info && row.info.error !== ""
                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.settingsWindow.rowPadding * 2 + Theme.controls.iconSize
                            Layout.rightMargin: Theme.settingsWindow.rowPadding
                            Layout.bottomMargin: Theme.settingsWindow.rowPadding
                            wrapMode: Text.WordWrap
                            color: Theme.controls.problemsText
                            font.pixelSize: Theme.controls.descriptionFontSize
                            text: row.info ? row.info.error : ""
                        }

                        Unfold {
                            id: details
                            open: row.expanded
                            Layout.fillWidth: true
                            leftPadding: Theme.settingsWindow.rowPadding
                            rightPadding: Theme.settingsWindow.rowPadding
                            topPadding: Theme.settingsWindow.rowPadding * 1.5
                            bottomPadding: Theme.settingsWindow.rowPadding * 1.5
                            lazyContent: Component {
                                Label {
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.RichText
                                    color: Theme.gameEditor.updateNotesText
                                    text: AppUpdate.notesHtml(row.modelData.notes !== "" ? row.modelData.notes : qsTr("No release notes."),
                                                              Qt.alpha(Theme.gameEditor.updateNotesText, 0.25))
                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                }
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
