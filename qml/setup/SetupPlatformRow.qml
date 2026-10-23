import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var platform
    property bool expanded: false
    property bool firstInCard: false
    property bool lastInCard: false
    readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                      Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)
    property string error: ""

    readonly property var foundEmulator: Library.emulators.find(e => e.found && root.platform.emulators.includes(e.id)) ?? null
    readonly property bool found: foundEmulator !== null
    // The emulator to install: one with a job already, else the first that can be installed.
    readonly property string installId: found ? ""
        : platform.emulators.find(id => (Installer.emulators[id]?.state ?? "idle") !== "idle")
          ?? platform.emulators.find(id => (Installer.emulators[id]?.routes.length ?? 0) > 0)
          ?? ""
    readonly property var named: foundEmulator
                                 ?? Library.emulators.find(e => e.id === (installId !== "" ? installId : root.platform.emulators[0]))
                                 ?? null

    readonly property var installInfo: installId !== "" ? Installer.emulators[installId] ?? null : null
    readonly property string jobState: installInfo?.state ?? "idle"
    readonly property bool hasRoutes: (installInfo?.routes.length ?? 0) > 0
    // Installed, and the library is still looking for it.
    readonly property bool awaitingDetection: !found && jobState === "idle" && (installInfo?.installedByApp ?? false)
    readonly property bool detecting: !found && (named?.detecting ?? false)

    // What EmulatorInstallTop reads from its row.
    readonly property var emulator: named ?? ({ id: "", preset: true })
    readonly property bool installing: jobState !== "idle" && jobState !== "failed"
    readonly property bool installFailed: jobState === "failed"
    readonly property bool canInstall: !installFailed && hasRoutes
    readonly property bool byApp: false
    readonly property bool hasUpdate: false

    readonly property string foldersText: {
        const n = platform.romFolders.length
        return n === 0 ? qsTr("No ROM folders") : n === 1 ? qsTr("1 ROM folder") : qsTr("%1 ROM folders").arg(n)
    }
    readonly property string subtitle: {
        const name = named?.name ?? ""
        return found || platform.romFolders.length > 0 ? name + " · " + foldersText : name
    }
    readonly property string badgeText: {
        if (found) {
            switch (foundEmulator.installKind) {
            case "flatpak": return qsTr("Flatpak")
            case "appimage": return qsTr("AppImage")
            case "executable": return qsTr("Installed")
            case "path": return qsTr("Your path")
            }
        }
        if (installFailed)
            return qsTr("Install failed")
        if (awaitingDetection)
            return qsTr("Looking for %1…").arg(named?.name ?? "")
        return detecting ? qsTr("Looking…") : qsTr("Not found")
    }
    readonly property color badgeColor: found ? Theme.settingsWindow.installFound
                                      : installFailed ? Theme.settingsWindow.installError
                                      : hasRoutes || awaitingDetection || detecting ? Theme.settingsWindow.installNotFound
                                      : Theme.settingsWindow.installMissing
    readonly property bool offerInstall: !found && !detecting && !expanded && canInstall && !installing && !awaitingDetection

    signal toggleRequested()

    spacing: 0

    function saveFolders(folders) {
        root.error = Library.setPlatformValue(root.platform.id, "rom_folders", folders)
    }

    Rectangle {
        id: header
        Layout.fillWidth: true
        implicitHeight: headerRow.implicitHeight + Theme.settingsWindow.rowPadding * 2
        radius: Theme.settingsWindow.rowRadius
        // Corners against the card's follow its curve, so the gap around them is even.
        topLeftRadius: root.firstInCard ? root.cardCornerRadius : radius
        topRightRadius: topLeftRadius
        bottomLeftRadius: root.lastInCard && details.progress === 0 ? root.cardCornerRadius : radius
        bottomRightRadius: bottomLeftRadius
        color: headerHover.hovered || header.activeFocus ? Theme.controls.hover : "transparent"
        activeFocusOnTab: true

        Keys.onReturnPressed: root.toggleRequested()
        Keys.onEnterPressed: root.toggleRequested()
        Keys.onSpacePressed: root.toggleRequested()

        HoverHandler { id: headerHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: { header.forceActiveFocus(); root.toggleRequested() } }

        RowLayout {
            id: headerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.settingsWindow.rowPadding
            spacing: 10

            Icon {
                path: Icons.chevron
                size: Theme.controls.iconSize
                color: Theme.controls.iconColor
                rotation: root.expanded ? 90 : 0

                Behavior on rotation { enabled: Motion.on; MotionNumber {} }
            }

            Icon {
                path: Icons.forPlatform(root.platform.icon)
                size: Theme.controls.iconSize + 4
                color: Theme.controls.labelColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.platform.name
                    color: Theme.controls.labelColor
                    font.pixelSize: Theme.controls.labelFontSize
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: root.subtitle
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                visible: !root.installing && !root.offerInstall
                implicitWidth: badgeLabel.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                implicitHeight: badgeLabel.implicitHeight + 2
                radius: Theme.settingsWindow.sourceTagRadius
                color: Qt.alpha(root.badgeColor, Theme.settingsWindow.sourceTagFillOpacity)
                border.color: root.badgeColor
                border.width: Theme.settingsWindow.sourceTagBorderWidth
                opacity: root.detecting ? 0.6 : 1

                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: root.badgeText
                    color: root.badgeColor
                    font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                }
            }
            // Opens the row: the route is chosen in its tiles.
            SettingsButton {
                visible: root.offerInstall
                text: qsTr("Install")
                iconPath: Icons.download
                tip: qsTr("Install %1").arg(root.named?.name ?? "")
                onClicked: root.toggleRequested()
            }
            InstallStatus {
                visible: root.installing
                info: root.installInfo
            }
        }
    }

    Unfold {
        id: details
        open: root.expanded
        Layout.fillWidth: true
        leftPadding: Theme.settingsWindow.rowPadding * 2 + Theme.controls.iconSize
        rightPadding: Theme.settingsWindow.rowPadding
        topPadding: Theme.settingsWindow.rowPadding * 1.5
        bottomPadding: Theme.settingsWindow.rowPadding * 1.5
        spacing: Theme.settingsWindow.rowPadding * 1.5

        EmulatorInstallTop {
            visible: !root.found && !root.awaitingDetection && (root.installing || root.installFailed || root.hasRoutes)
            Layout.fillWidth: true
            row: root
        }

        EmulatorProgramField {
            visible: !root.found && !root.installing && root.named !== null
            Layout.fillWidth: true
            emulator: root.emulator
            title: root.hasRoutes ? qsTr("Already have it somewhere else?") : qsTr("Program")
            onSaved: error => root.error = error
        }

        Section {
            visible: root.found
            title: qsTr("ROM folders")
            actions: ListAction {
                text: qsTr("Add folder")
                tip: qsTr("Its games are found now and at every start, subfolders included")
                onClicked: folderDialog.active = true
            }

            Repeater {
                model: root.platform.romFolders

                ListRow {
                    id: folderItem
                    required property string modelData
                    required property int index
                    last: index === root.platform.romFolders.length - 1

                    Label {
                        Layout.fillWidth: true
                        text: folderItem.modelData
                        color: Theme.controls.inputText
                        font.family: Theme.font.monoFamily
                        font.pixelSize: Theme.controls.pathFontSize
                        elide: Text.ElideMiddle
                    }
                    ListButton {
                        iconPath: Icons.close
                        tip: qsTr("Stop scanning this folder")
                        onClicked: root.saveFolders(root.platform.romFolders.filter((f, i) => i !== folderItem.index))
                    }
                }
            }
            ListRow {
                visible: root.platform.romFolders.length === 0
                last: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("None yet: add the folder this platform's ROMs are in.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        Label {
            visible: root.error !== ""
            Layout.fillWidth: true
            text: root.error
            color: Theme.controls.problemsText
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }
    }

    Loader {
        id: folderDialog
        active: false
        sourceComponent: RomFolderDialog {
            title: qsTr("Add a ROM folder to %1").arg(root.platform.name)
            onPicked: path => {
                if (!root.platform.romFolders.includes(path))
                    root.saveFolders(root.platform.romFolders.concat([path]))
            }
            onClosed: folderDialog.active = false
        }
    }
}
