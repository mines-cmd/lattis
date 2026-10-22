import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// What heads an emulator's expanded card: the job running, why the last one failed, the routes to install
// it by, or where Lattis's own install came from. `row` is the EmulatorRow.
ColumnLayout {
    id: head

    required property var row

    readonly property var info: row.installInfo
    readonly property var routes: info?.routes ?? []
    readonly property bool downloadKnown: info?.state === "downloading" && info.size > 0 && info.progress >= 0

    function routeName(route) {
        return route === "flatpak" ? qsTr("Flatpak") : qsTr("AppImage")
    }
    function routeOrigin(route) {
        return route === "flatpak" ? qsTr("From Flathub") : qsTr("From GitHub, %1").arg(info?.project ?? "")
    }
    function routeMeaning(route) {
        return route === "flatpak" ? qsTr("Sandboxed. Updated by your system.")
                                   : qsTr("One file in ~/Applications. Lattis checks it for updates.")
    }

    readonly property string jobTitle: {
        switch (info?.state) {
        case "checking": return qsTr("Checking the latest release")
        case "downloading": return qsTr("Downloading the AppImage from %1").arg(info.source)
        case "extracting": return qsTr("Unpacking")
        case "installing":
            return info.jobRoute === "flatpak" ? qsTr("Installing the Flatpak from Flathub") : qsTr("Installing the AppImage")
        case "removing": return qsTr("Uninstalling")
        }
        return ""
    }
    readonly property string jobDetail: {
        if (downloadKnown) {
            const mb = bytes => Math.max(1, Math.round(bytes / 1e6))
            return qsTr("%1 of %2 MB, %3 %").arg(mb(info.size * info.progress)).arg(mb(info.size)).arg(Math.round(info.progress * 100))
        }
        return info?.state === "installing" && info.jobRoute === "flatpak" ? qsTr("Flatpak doesn't report progress") : ""
    }
    readonly property string sourceTitle: {
        if (!row.byApp)
            return ""
        let title = routeName(info.route)
        if (info.version !== "")
            title += " " + info.version
        if (row.hasUpdate)
            return title + ", " + qsTr("%1 is out").arg(info.updateVersion)
        return info.upToDate ? title + ", " + qsTr("up to date") : title
    }

    spacing: Theme.settingsWindow.rowPadding * 1.5

    Rectangle {
        visible: row.installing
        Layout.fillWidth: true
        implicitHeight: jobColumn.implicitHeight + Theme.settingsWindow.installTilePadding * 2
        radius: Theme.settingsWindow.installTileRadius
        color: Theme.controls.inputBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.settingsWindow.installTilePadding
            spacing: Theme.controls.controlSpacing

            ColumnLayout {
                id: jobColumn
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: head.jobTitle
                        color: Theme.controls.labelColor
                        font.weight: Theme.font.strongWeight
                        elide: Text.ElideRight
                    }
                    Label {
                        visible: head.jobDetail !== ""
                        text: head.jobDetail
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                    }
                }
                InstallProgress {
                    Layout.fillWidth: true
                    progress: head.downloadKnown ? head.info.progress : -1
                }
            }
            SettingsButton {
                enabled: head.info?.canCancel ?? false
                text: qsTr("Cancel")
                onClicked: Installer.cancel(row.emulator.id)
            }
        }
    }

    Rectangle {
        visible: row.installFailed
        Layout.fillWidth: true
        implicitHeight: failedRow.implicitHeight + Theme.settingsWindow.installTilePadding * 2
        radius: Theme.settingsWindow.installTileRadius
        color: Qt.alpha(Theme.settingsWindow.installError, Theme.settingsWindow.installErrorFillOpacity)
        border.color: Theme.settingsWindow.installError
        border.width: Theme.controls.inputBorderWidth

        RowLayout {
            id: failedRow
            anchors.fill: parent
            anchors.margins: Theme.settingsWindow.installTilePadding
            spacing: Theme.controls.controlSpacing

            Label {
                Layout.fillWidth: true
                text: head.info?.error ?? ""
                color: Theme.settingsWindow.installError
                wrapMode: Text.WordWrap
            }
            SettingsButton {
                text: qsTr("Try again")
                onClicked: Installer.retry(row.emulator.id)
            }
            SettingsButton {
                text: qsTr("Dismiss")
                onClicked: Installer.cancel(row.emulator.id)
            }
        }
    }

    ColumnLayout {
        visible: row.canInstall && !row.installing
        Layout.fillWidth: true
        spacing: 6

        GridLayout {
            id: tiles
            Layout.fillWidth: true
            columns: width >= Theme.settingsWindow.platformColumnsMinWidth ? 2 : 1
            columnSpacing: Theme.controls.controlSpacing
            rowSpacing: Theme.controls.controlSpacing

            Item {
                visible: head.routes.length === 1 && tiles.columns === 2
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.row: 0
                Layout.column: 1
            }
            Repeater {
                model: head.routes

                Rectangle {
                    id: tile
                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: tileColumn.implicitHeight + Theme.settingsWindow.installTilePadding * 2
                    radius: Theme.settingsWindow.installTileRadius
                    color: Theme.controls.inputBackground
                    border.color: Theme.controls.inputBorder
                    border.width: Theme.controls.inputBorderWidth

                    ColumnLayout {
                        id: tileColumn
                        anchors.fill: parent
                        anchors.margins: Theme.settingsWindow.installTilePadding
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            Label {
                                text: head.routeName(tile.modelData)
                                color: Theme.controls.labelColor
                                font.weight: Theme.font.strongWeight
                            }
                            Rectangle {
                                visible: tile.index === 0 && head.routes.length > 1
                                implicitWidth: recommended.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                                implicitHeight: recommended.implicitHeight + 2
                                radius: Theme.settingsWindow.sourceTagRadius
                                color: Qt.alpha(Theme.settingsWindow.installFound, Theme.settingsWindow.sourceTagFillOpacity)
                                border.color: Theme.settingsWindow.installFound
                                border.width: Theme.settingsWindow.sourceTagBorderWidth

                                Label {
                                    id: recommended
                                    anchors.centerIn: parent
                                    text: qsTr("Recommended")
                                    color: Theme.settingsWindow.installFound
                                    font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                                }
                            }
                        }
                        Label {
                            text: head.routeOrigin(tile.modelData)
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                        }
                        Label {
                            Layout.fillWidth: true
                            text: head.routeMeaning(tile.modelData)
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                        SettingsButton {
                            Layout.topMargin: 4
                            primary: tile.index === 0
                            text: qsTr("Install %1").arg(head.routeName(tile.modelData))
                            onClicked: Installer.install(row.emulator.id, tile.modelData)
                        }
                    }
                }
            }
        }
        Label {
            readonly property bool noFlatpak: head.info?.noFlatpak ?? false
            visible: row.emulator.preset && (noFlatpak || !head.routes.includes("appimage"))
            Layout.fillWidth: true
            text: noFlatpak ? qsTr("Flatpak isn't installed on this system") : qsTr("No official AppImage")
            color: Theme.controls.descriptionColor
            font.pixelSize: Theme.controls.descriptionFontSize
        }
    }

    Rectangle {
        visible: row.byApp && !row.installing
        Layout.fillWidth: true
        implicitHeight: sourceRow.implicitHeight + Theme.settingsWindow.installTilePadding * 2
        radius: Theme.settingsWindow.installTileRadius
        color: Theme.controls.inputBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth

        RowLayout {
            id: sourceRow
            anchors.fill: parent
            anchors.margins: Theme.settingsWindow.installTilePadding
            spacing: Theme.controls.controlSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: head.sourceTitle
                    color: Theme.controls.labelColor
                    font.weight: Theme.font.strongWeight
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: row.byApp ? qsTr("Installed by Lattis on %1 from %2.")
                                          .arg(new Date(info.installed + "T00:00").toLocaleDateString(Qt.locale(), General.shortDateFormat))
                                          .arg(info.route === "flatpak" ? qsTr("Flathub") : info.project)
                                    : ""
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
            }
            SettingsButton {
                visible: row.hasUpdate
                primary: true
                text: qsTr("Update to %1").arg(head.info?.updateVersion ?? "")
                tip: qsTr("Download it and move the current AppImage to the trash")
                onClicked: Installer.update(row.emulator.id)
            }
            SettingsButton {
                visible: !row.hasUpdate && head.info?.route === "appimage"
                text: qsTr("Check for updates")
                onClicked: Installer.checkUpdates(row.emulator.id)
            }
        }
    }
}
