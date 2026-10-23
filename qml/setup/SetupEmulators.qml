import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide
    // With nothing installed the short list would be empty, and every platform's Install is what's useful.
    property bool showAll: installed.length === 0

    // Only platforms with an emulator: a ROM folder is only useful once one can launch it.
    readonly property var withEmulator: Library.platformSettings.filter(p => p.emulators.length > 0)
    readonly property var installed: withEmulator.filter(p => root.emulatorFound(p))
    readonly property var notInstalled: withEmulator.filter(p => !root.emulatorFound(p))
    // Platforms whose emulator was installed from this step keep their place when it's found, instead of jumping up.
    property var startedHere: ({})
    readonly property var shown: root.showAll
        ? withEmulator.filter(p => root.emulatorFound(p) && !root.startedHere[p.id])
              .concat(withEmulator.filter(p => !root.emulatorFound(p) || root.startedHere[p.id]))
        : installed
    readonly property bool canInstallAny: notInstalled.some(p => root.installId(p) !== "")

    spacing: Theme.onboarding.spacing

    Component.onCompleted: Installer.refresh()

    readonly property Component headerAction: SettingsButton {
        text: qsTr("Find from one folder")
        tip: qsTr("Match a folder's subfolders to platforms by name")
        iconPath: Icons.folder
        onClicked: findDialog.active = true
    }

    function emulatorOf(p) {
        return Library.emulators.find(e => e.id === p.emulators[0])
    }
    // The emulator to install for a platform: one with a job already, else the first that can be installed.
    function installId(p) {
        return p.emulators.find(id => (Installer.emulators[id]?.state ?? "idle") !== "idle")
            ?? p.emulators.find(id => (Installer.emulators[id]?.routes.length ?? 0) > 0)
            ?? ""
    }
    // Platform ids whose install card is open; kept here because the rows are rebuilt when the list changes.
    property var expandedIds: ({})
    function toggle(platformId) {
        const next = Object.assign({}, root.expandedIds)
        next[platformId] = !next[platformId]
        root.expandedIds = next
    }
    function markStarted(platformId) {
        if (root.startedHere[platformId])
            return
        const next = Object.assign({}, root.startedHere)
        next[platformId] = true
        root.startedHere = next
        root.showAll = true
    }
    function emulatorFound(p) {
        return p.emulators.some(id => { const e = Library.emulators.find(e => e.id === id); return e && e.found })
    }
    function installLabel(p) {
        const e = root.emulatorOf(p)
        if (!e)
            return ""
        switch (e.installKind) {
        case "flatpak": return qsTr("%1 · Flatpak").arg(e.name)
        case "appimage": return qsTr("%1 · AppImage").arg(e.name)
        case "executable": return qsTr("%1 · installed").arg(e.name)
        case "path": return qsTr("%1 · your path").arg(e.name)
        }
        return e.detecting ? qsTr("Looking for %1…").arg(e.name) : qsTr("%1 not found").arg(e.name)
    }
    function save(id, folders) {
        const error = Library.setPlatformValue(id, "rom_folders", folders)
        if (error !== "")
            guide.message(error)
    }
    function removed(list, i) {
        const out = list.slice()
        out.splice(i, 1)
        return out
    }

    Loader {
        id: findDialog
        property string pendingFolder: ""
        active: false
        sourceComponent: FindRomFoldersDialog {
            initialFolder: findDialog.pendingFolder
            onClosed: findDialog.active = false
        }
    }

    function openFindDialogForScreenshot(path) {
        findDialog.pendingFolder = path
        findDialog.active = true
    }

    SettingsCard {
        panel: true
        visible: root.shown.length > 0
        spacing: 0

        Repeater {
            model: root.shown

            ColumnLayout {
                id: entry
                required property var modelData
                required property int index
                readonly property bool platformInstalled: root.emulatorFound(modelData)
                readonly property string installId: platformInstalled ? "" : root.installId(modelData)
                readonly property var installInfo: installId !== "" ? Installer.emulators[installId] ?? null : null
                readonly property string jobState: installInfo?.state ?? "idle"
                readonly property bool running: jobState !== "idle" && jobState !== "failed"
                // Installed, and the library is still looking for it.
                readonly property bool awaitingDetection: jobState === "idle" && (installInfo?.installedByApp ?? false)
                readonly property string neededName: Library.emulators.find(e => e.id === installId)?.name
                                                     ?? root.emulatorOf(modelData)?.name ?? ""
                readonly property bool expandable: !platformInstalled && !awaitingDetection
                                                   && (running || jobState === "failed" || (installInfo?.routes.length ?? 0) > 0)
                readonly property bool expanded: expandable && (root.expandedIds[modelData.id] ?? false)

                // What EmulatorInstallTop reads from its row.
                readonly property var emulator: Library.emulators.find(e => e.id === installId) ?? ({ id: "", preset: true })
                readonly property bool installing: running
                readonly property bool installFailed: jobState === "failed"
                readonly property bool canInstall: jobState !== "failed" && (installInfo?.routes.length ?? 0) > 0
                readonly property bool byApp: false
                readonly property bool hasUpdate: false

                readonly property string badgeText: {
                    if (platformInstalled)
                        return root.installLabel(modelData)
                    if (installFailed)
                        return qsTr("Install failed")
                    if (awaitingDetection)
                        return qsTr("Looking for %1…").arg(neededName)
                    return qsTr("Needs %1").arg(neededName)
                }
                readonly property color badgeColor: platformInstalled ? Theme.settingsWindow.installFound
                                                  : installFailed ? Theme.settingsWindow.installError
                                                  : expandable || awaitingDetection ? Theme.settingsWindow.installNotFound
                                                  : Theme.settingsWindow.installMissing

                onJobStateChanged: if (running) root.markStarted(modelData.id)

                Layout.fillWidth: true
                spacing: Theme.onboarding.controlSpacing

                Rectangle {
                    visible: entry.index > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    Layout.bottomMargin: 4
                    implicitHeight: 1
                    color: Theme.settingsWindow.rowDivider
                }

                Item {
                    id: header
                    Layout.fillWidth: true
                    implicitHeight: headerRow.implicitHeight
                    activeFocusOnTab: entry.expandable

                    Keys.onReturnPressed: root.toggle(entry.modelData.id)
                    Keys.onEnterPressed: root.toggle(entry.modelData.id)
                    Keys.onSpacePressed: root.toggle(entry.modelData.id)

                    HoverHandler {
                        id: headerHover
                        enabled: entry.expandable
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        enabled: entry.expandable
                        onTapped: { header.forceActiveFocus(); root.toggle(entry.modelData.id) }
                    }

                    // Past the row's edges, so the highlight doesn't move what's in it.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -Theme.settingsWindow.rowPadding / 2
                        radius: Theme.settingsWindow.rowRadius
                        color: Theme.controls.hover
                        visible: entry.expandable && (headerHover.hovered || header.activeFocus)
                    }

                    RowLayout {
                        id: headerRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 10

                        Icon {
                            path: Icons.forPlatform(entry.modelData.icon)
                            size: Theme.controls.iconSize + 4
                            color: Theme.controls.labelColor
                        }
                        Label {
                            text: entry.modelData.name
                            color: Theme.controls.labelColor
                            font.pixelSize: Theme.controls.labelFontSize
                            font.weight: Theme.font.strongWeight
                        }
                        Rectangle {
                            visible: entry.platformInstalled
                            implicitWidth: installedText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                            implicitHeight: installedText.implicitHeight + 2
                            radius: Theme.settingsWindow.sourceTagRadius
                            color: Qt.alpha(entry.badgeColor, Theme.settingsWindow.sourceTagFillOpacity)
                            border.color: entry.badgeColor
                            border.width: Theme.settingsWindow.sourceTagBorderWidth

                            Label {
                                id: installedText
                                anchors.centerIn: parent
                                text: entry.badgeText
                                color: entry.badgeColor
                                font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            visible: !entry.platformInstalled && !entry.running
                            implicitWidth: statusText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                            implicitHeight: statusText.implicitHeight + 2
                            radius: Theme.settingsWindow.sourceTagRadius
                            color: Qt.alpha(entry.badgeColor, Theme.settingsWindow.sourceTagFillOpacity)
                            border.color: entry.badgeColor
                            border.width: Theme.settingsWindow.sourceTagBorderWidth

                            Label {
                                id: statusText
                                anchors.centerIn: parent
                                text: entry.badgeText
                                color: entry.badgeColor
                                font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                            }
                        }
                        InstallStatus {
                            visible: entry.running
                            info: entry.installInfo
                        }
                        Icon {
                            visible: entry.expandable
                            path: Icons.chevron
                            size: Theme.controls.iconSize
                            color: Theme.controls.iconColor
                            rotation: entry.expanded ? 90 : 0

                            Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                        }
                        SettingsButton {
                            visible: entry.platformInstalled
                            text: qsTr("Add folder")
                            iconPath: Icons.add
                            onClicked: {
                                folderDialog.active = true
                                folderDialog.platformId = entry.modelData.id
                                folderDialog.folders = entry.modelData.romFolders
                            }
                        }
                    }
                }

                Unfold {
                    open: entry.expanded
                    Layout.fillWidth: true
                    leftPadding: Theme.controls.iconSize + 14
                    topPadding: Theme.settingsWindow.rowPadding / 2

                    EmulatorInstallTop {
                        Layout.fillWidth: true
                        row: entry
                    }
                }

                ColumnLayout {
                    visible: entry.platformInstalled
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.controls.iconSize + 14
                    spacing: 6

                    Repeater {
                        model: entry.modelData.romFolders

                        RowLayout {
                            id: folderRow
                            required property string modelData
                            required property int index

                            Layout.fillWidth: true
                            spacing: Theme.onboarding.controlSpacing

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: pathLabel.implicitHeight + 12
                                radius: Theme.controls.inputRadius
                                color: Theme.controls.inputBackground

                                Label {
                                    id: pathLabel
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 10
                                    text: folderRow.modelData
                                    color: Theme.controls.inputText
                                    font.family: Theme.font.monoFamily
                                    font.pixelSize: Theme.controls.pathFontSize
                                    elide: Text.ElideMiddle
                                }
                            }
                            SettingsButton {
                                implicitHeight: pathLabel.implicitHeight + 12
                                leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                                rightPadding: leftPadding
                                iconPath: Icons.close
                                tip: qsTr("Stop scanning this folder")
                                onClicked: root.save(entry.modelData.id, root.removed(entry.modelData.romFolders, folderRow.index))
                            }
                        }
                    }

                    Label {
                        visible: entry.modelData.romFolders.length === 0
                        Layout.fillWidth: true
                        text: qsTr("No ROM folders yet.")
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                    }
                }
            }
        }
    }

    RowLayout {
        visible: root.notInstalled.length > 0
        Layout.fillWidth: true
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: root.showAll
                  ? qsTr("Platforms without an installed emulator are shown too.")
                  : (root.notInstalled.length === 1
                     ? qsTr("1 more platform has no emulator installed here (%1).").arg(root.notInstalled[0].name)
                     : qsTr("%1 more platforms have no emulator installed here (%2).")
                           .arg(root.notInstalled.length).arg(root.notInstalled.map(p => p.name).join(", ")))
                    + (root.canInstallAny ? " " + qsTr("Show them to install an emulator from here.") : "")
            color: Theme.onboarding.descriptionColor
            wrapMode: Text.WordWrap
        }
        SettingsButton {
            text: root.showAll ? qsTr("Show installed only") : qsTr("Show all platforms")
            onClicked: root.showAll = !root.showAll
        }
    }

    Loader {
        id: folderDialog
        property string platformId: ""
        property var folders: []
        active: false
        sourceComponent: RomFolderDialog {
            onPicked: path => {
                if (!folderDialog.folders.includes(path))
                    root.save(folderDialog.platformId, folderDialog.folders.concat([path]))
            }
            onClosed: folderDialog.active = false
        }
    }

    Item { Layout.fillHeight: true }
}
