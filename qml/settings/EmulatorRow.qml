import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components/Args.js" as Args

ColumnLayout {
    id: root

    required property var emulator
    property bool expanded: false
    property bool firstInCard: false
    property bool lastInCard: false
    readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                      Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)
    property string error: ""

    readonly property bool own: !emulator.preset
    readonly property var platformChoices: {
        const choices = Library.platformSettings.map(p => ({ id: p.id, name: p.name }))
        for (const id of emulator.platformIds) {
            if (!choices.some(c => c.id === id))
                choices.push({ id: id, name: id })
        }
        return choices
    }
    readonly property var unusedPlatforms: platformChoices.filter(c => !emulator.platformIds.includes(c.id))
    readonly property string kindLabel: {
        switch (emulator.installKind) {
        case "flatpak": return qsTr("Flatpak")
        case "appimage": return qsTr("AppImage")
        case "executable": return qsTr("Installed")
        case "path": return qsTr("Your path")
        }
        return emulator.detecting ? qsTr("Looking…") : qsTr("Not found")
    }

    // The emulator's Installer row: null for one that can't be installed from here.
    readonly property var installInfo: Installer.emulators[emulator.id] ?? null
    readonly property string installState: installInfo?.state ?? "idle"
    readonly property bool installing: installState !== "idle" && installState !== "failed"
    readonly property bool installFailed: installState === "failed"
    readonly property bool byApp: installInfo?.installedByApp ?? false
    readonly property bool canInstall: !emulator.found && !emulator.detecting && (installInfo?.routes.length ?? 0) > 0
    readonly property bool hasUpdate: byApp && (installInfo?.updateVersion ?? "") !== ""
    readonly property string otherRoute: {
        if (!byApp)
            return ""
        const other = installInfo.route === "flatpak" ? "appimage" : "flatpak"
        return installInfo.routes.includes(other) ? other : ""
    }
    // Says who installed it and which version, for the subtitle; the badge shows the route.
    readonly property string sourceSummary: {
        if (!byApp || !emulator.found)
            return ""
        const label = qsTr("Installed by Lattis")
        return installInfo.version !== "" ? label + " · " + installInfo.version : label
    }
    readonly property string badgeText: {
        if (installFailed)
            return qsTr("Install failed")
        if (hasUpdate)
            return qsTr("Update %1").arg(installInfo.updateVersion)
        return kindLabel
    }
    readonly property color badgeColor: {
        if (installFailed)
            return Theme.settingsWindow.installError
        return emulator.found ? Theme.settingsWindow.installFound : Theme.settingsWindow.installNotFound
    }

    signal toggleRequested()
    signal openFileRequested()

    spacing: 0

    function save(key, value) {
        root.error = Library.setEmulatorValue(root.emulator.id, key, value)
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.emulator.name
                    color: Theme.controls.labelColor
                    font.pixelSize: Theme.controls.labelFontSize
                    elide: Text.ElideRight

                    HoverHandler { id: nameHover }
                    ToolTip.visible: nameHover.hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: qsTr("Platforms and games refer to it as “%1”").arg(root.emulator.id)
                }
                Label {
                    id: subtitle
                    readonly property string platforms: root.emulator.platforms.length > 0 ? root.emulator.platforms.join(", ")
                                                                                           : qsTr("No platforms")
                    Layout.fillWidth: true
                    text: root.sourceSummary !== "" ? platforms + " · " + root.sourceSummary : platforms
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    elide: Text.ElideRight

                    HoverHandler { id: subtitleHover }
                    ToolTip.visible: subtitleHover.hovered && subtitle.truncated
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: subtitle.text
                }
            }

            Rectangle {
                visible: !root.installing
                implicitWidth: kindText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                implicitHeight: kindText.implicitHeight + 2
                radius: Theme.settingsWindow.sourceTagRadius
                color: Qt.alpha(root.badgeColor, Theme.settingsWindow.sourceTagFillOpacity)
                border.color: root.badgeColor
                border.width: Theme.settingsWindow.sourceTagBorderWidth
                opacity: root.emulator.detecting ? 0.6 : 1

                Label {
                    id: kindText
                    anchors.centerIn: parent
                    text: root.badgeText
                    color: root.badgeColor
                    font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                }
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
            visible: !root.own && (root.installing || root.installFailed || root.canInstall || root.byApp)
            Layout.fillWidth: true
            row: root
        }

        ColumnLayout {
            visible: root.own
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true

                SectionTitle {
                    Layout.fillWidth: true
                    text: qsTr("Name")
                }
                // The id, which the name isn't: renaming changes only what the app shows.
                AbstractButton {
                    id: idHelp
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    background: null
                    contentItem: Icon {
                        path: Icons.help
                        size: Theme.controls.descriptionFontSize
                        color: idHelp.hovered ? Theme.controls.labelColor : Theme.controls.descriptionColor
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: qsTr("Platforms and games refer to it as \"%1\", which stays when you rename it.").arg(root.emulator.id)
                }
            }
            SettingsTextField {
                id: nameField
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                text: root.emulator.name
                onEditingFinished: {
                    if (text.trim() === root.emulator.name)
                        return
                    root.save("name", text)
                    if (root.error !== "")
                        text = root.emulator.name
                }
                Connections {
                    target: root
                    function onEmulatorChanged() { if (!nameField.activeFocus) nameField.text = root.emulator.name }
                }
            }
        }

        GridLayout {
            visible: root.own
            Layout.fillWidth: true
            columns: width >= Theme.settingsWindow.platformColumnsMinWidth ? 2 : 1
            columnSpacing: Theme.settingsWindow.rowPadding * 2
            rowSpacing: Theme.settingsWindow.rowPadding * 1.5

            Section {
                title: qsTr("Platforms")
                note: qsTr("Settings → Platforms says which emulators each platform uses.")
                actions: ListAction {
                    id: addPlatform
                    text: qsTr("Add platform")
                    onClicked: platformsMenu.popup(addPlatform, addPlatform.width - platformsMenu.width, addPlatform.height)

                    SettingsMenu {
                        id: platformsMenu

                        Instantiator {
                            model: root.unusedPlatforms.length > 0
                                   ? root.unusedPlatforms
                                   : [{ id: "", name: qsTr("It runs every platform already") }]
                            delegate: Action {
                                required property var modelData
                                text: modelData.name
                                enabled: modelData.id !== ""
                                onTriggered: root.save("platforms", root.emulator.platformIds.concat([modelData.id]))
                            }
                            onObjectAdded: (index, object) => platformsMenu.insertAction(index, object)
                            onObjectRemoved: (index, object) => platformsMenu.removeAction(object)
                        }
                    }
                }

                Repeater {
                    model: root.emulator.platformIds

                    ListRow {
                        id: platformItem
                        required property string modelData
                        required property int index
                        last: index === root.emulator.platformIds.length - 1

                        Label {
                            Layout.fillWidth: true
                            text: root.platformChoices.find(c => c.id === platformItem.modelData)?.name ?? platformItem.modelData
                            color: Theme.controls.labelColor
                            elide: Text.ElideRight
                        }
                        ListButton {
                            iconPath: Icons.close
                            tip: qsTr("It no longer runs this platform's games")
                            onClicked: root.save("platforms", root.emulator.platformIds.filter(id => id !== platformItem.modelData))
                        }
                    }
                }
                ListRow {
                    visible: root.emulator.platformIds.length === 0
                    last: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("None yet: add the platforms it runs games for.")
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6

                SectionTitle { text: qsTr("ROM extensions") }
                SettingsTextField {
                    id: extensionsField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    text: root.emulator.extensions.join(" ")
                    placeholderText: qsTr("e.g. 3ds cci cxi")
                    onEditingFinished: {
                        const extensions = text.split(/[\s,]+/).filter(e => e !== "")
                        if (extensions.join(" ").toLowerCase().replace(/\./g, "") !== root.emulator.extensions.join(" "))
                            root.save("extensions", extensions)
                    }
                    Connections {
                        target: root
                        function onEmulatorChanged() {
                            if (!extensionsField.activeFocus) extensionsField.text = root.emulator.extensions.join(" ")
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("The files in ROM folders that are its games, separated by spaces.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            SectionTitle { text: root.canInstall && !root.installing ? qsTr("Already have it somewhere else?") : qsTr("Program") }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                SettingsTextField {
                    id: pathField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    text: root.emulator.path
                    placeholderText: root.emulator.found && root.emulator.install !== "" ? root.emulator.install
                                   : root.emulator.found ? qsTr("Found automatically")
                                                         : qsTr("Not found; choose the program")
                    rightPadding: copyPath.visible ? copyPath.width : leftPadding
                    onEditingFinished: if (text !== root.emulator.path) root.save("path", text)
                    // A new value from the file: show it unless it's being typed over.
                    Connections {
                        target: root
                        function onEmulatorChanged() { if (!pathField.activeFocus) pathField.text = root.emulator.path }
                    }

                    AbstractButton {
                        id: copyPath
                        readonly property string value: pathField.text !== "" ? pathField.text
                                                        : root.emulator.found ? root.emulator.install : ""
                        property bool copied: false

                        visible: value !== ""
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: height
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        background: Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(1, Theme.controls.inputBorderWidth)
                            height: parent.height / 2
                            color: Theme.controls.inputBorder
                        }
                        contentItem: Item {
                            Icon {
                                anchors.centerIn: parent
                                path: copyPath.copied ? Icons.check : Icons.copy
                                size: Theme.controls.iconSize
                                color: copyPath.hovered ? Theme.controls.inputText : Theme.controls.descriptionColor
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: copied ? qsTr("Copied") : qsTr("Copy the full path")
                        HoverHandler { cursorShape: Qt.PointingHandCursor }

                        TextEdit { id: clipboard; visible: false }
                        Timer { id: copiedReset; interval: 1500; onTriggered: copyPath.copied = false }
                        onClicked: {
                            clipboard.text = value
                            clipboard.selectAll()
                            clipboard.copy()
                            copied = true
                            copiedReset.restart()
                        }
                    }
                }
                SettingsButton {
                    text: qsTr("Browse")
                    tip: qsTr("Choose the emulator's program or AppImage")
                    onClicked: pathDialog.active = true
                }
                SettingsButton {
                    visible: root.emulator.path !== ""
                    text: qsTr("Detect automatically")
                    tip: qsTr("Forget this path and look for the Flatpak, the program or an AppImage")
                    onClicked: root.save("path", "")
                }
            }
            Label {
                id: pathNote
                readonly property bool noProgram: root.own && !root.emulator.found && root.emulator.path === ""
                                                  && root.emulator.flatpak === ""
                visible: root.emulator.pathMissing || noProgram
                Layout.fillWidth: true
                text: noProgram ? qsTr("Choose its program, or give its Flatpak app id below.")
                    : root.emulator.found ? qsTr("Nothing is at this path, so the one found automatically is used.")
                                          : qsTr("Nothing is at this path.")
                color: Theme.settingsWindow.installMissing
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            visible: root.own
            Layout.fillWidth: true
            spacing: 6

            SectionTitle { text: qsTr("Flatpak app id") }
            SettingsTextField {
                id: flatpakField
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                text: root.emulator.flatpak
                placeholderText: qsTr("e.g. org.citra_emu.citra")
                onEditingFinished: if (text.trim() !== root.emulator.flatpak) root.save("flatpak", text)
                Connections {
                    target: root
                    function onEmulatorChanged() { if (!flatpakField.activeFocus) flatpakField.text = root.emulator.flatpak }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.own && width >= Theme.settingsWindow.platformColumnsMinWidth ? 2 : 1
            columnSpacing: Theme.settingsWindow.rowPadding * 2
            rowSpacing: Theme.settingsWindow.rowPadding * 1.5

            ColumnLayout {
                visible: root.own
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6

                SectionTitle { text: qsTr("Command") }
                SettingsTextField {
                    id: commandField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    text: Args.join(root.emulator.commandArgs)
                    placeholderText: "{exe} {args} {rom}"
                    onEditingFinished: {
                        const command = Args.split(text)
                        if (Args.join(command) === Args.join(root.emulator.commandArgs))
                            return
                        root.save("command", command)
                        if (root.error !== "")
                            text = Args.join(root.emulator.commandArgs)
                    }
                    Connections {
                        target: root
                        function onEmulatorChanged() {
                            if (!commandField.activeFocus) commandField.text = Args.join(root.emulator.commandArgs)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("{exe} is the program, {args} the arguments, {rom} the game.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                visible: root.own || root.emulator.found
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    SectionTitle {
                        Layout.fillWidth: true
                        text: qsTr("Arguments")
                    }
                    ListAction {
                        visible: !root.own && root.emulator.custom.includes("args")
                        iconPath: Icons.undo
                        text: qsTr("Reset")
                        tip: qsTr("Back to the built-in arguments")
                        onClicked: root.save("args", undefined)
                    }
                }
                SettingsTextField {
                    id: argsField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    text: Args.join(root.emulator.args)
                    placeholderText: qsTr("None")
                    onEditingFinished: {
                        const args = Args.split(text)
                        if (Args.join(args) !== Args.join(root.emulator.args))
                            root.save("args", args)
                    }
                    Connections {
                        target: root
                        function onEmulatorChanged() { if (!argsField.activeFocus) argsField.text = Args.join(root.emulator.args) }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Added for every game, like --fullscreen.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        ColumnLayout {
            visible: root.own || root.emulator.found
            Layout.fillWidth: true
            spacing: 6

            SectionTitle { text: qsTr("Runs") }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: runsText.implicitHeight + 12
                radius: Theme.controls.inputRadius
                color: Theme.controls.inputBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth

                TextEdit {
                    id: runsText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 8
                    text: root.emulator.command
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: Theme.controls.descriptionColor
                    selectionColor: Theme.controls.inputBorderFocus
                    font.family: Theme.font.monoFamily
                    font.pixelSize: Theme.controls.descriptionFontSize
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

        ColumnLayout {
            visible: root.byApp
            Layout.fillWidth: true
            spacing: Theme.settingsWindow.rowPadding

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Math.max(1, Theme.controls.inputBorderWidth)
                color: Theme.controls.inputBorder
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                SettingsButton {
                    visible: root.otherRoute !== ""
                    flat: true
                    enabled: !root.installing
                    text: root.otherRoute === "flatpak" ? qsTr("Install the Flatpak instead") : qsTr("Install the AppImage instead")
                    tip: root.otherRoute === "flatpak" ? qsTr("Flatpak from Flathub, updated by your system")
                                                       : qsTr("AppImage from %1, the newest build").arg(root.installInfo?.source ?? "")
                    onClicked: Installer.install(root.emulator.id, root.otherRoute)
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Your saves and keys stay.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                }
                SettingsButton {
                    danger: true
                    enabled: !root.installing
                    text: qsTr("Uninstall")
                    onClicked: uninstallDialog.active = true
                }
            }
        }

        Label {
            visible: root.own
            Layout.fillWidth: true
            text: root.emulator.fileOnly.length > 0
                  ? qsTr("Also set in emulators.toml: %1.").arg(root.emulator.fileOnly.join(", "))
                  : qsTr("Saves and keys, for backups, are set in emulators.toml.")
            color: Theme.controls.descriptionColor
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }
        RowLayout {
            visible: root.own
            Layout.alignment: Qt.AlignRight
            spacing: Theme.controls.controlSpacing

            SettingsButton {
                text: qsTr("Open emulators.toml")
                iconPath: Icons.openExternal
                onClicked: root.openFileRequested()
            }
            SettingsButton {
                danger: true
                text: qsTr("Remove emulator")
                tip: qsTr("Platforms stop using it; games pinned to it use their platform's emulators")
                onClicked: removeDialog.active = true
            }
        }
    }

    Loader {
        id: removeDialog
        active: false
        sourceComponent: MotionPopup {
            id: popup

            readonly property int pinned: Library.pinnedGames(root.emulator.id)
            property string error: ""

            Component.onCompleted: open()
            onClosed: removeDialog.active = false

            parent: Overlay.overlay
            anchors.centerIn: parent
            modal: true
            Overlay.modal: Scrim {}
            padding: Theme.menu.dialogPadding
            width: 380
            background: Rectangle {
                color: Theme.menu.popupBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth
                radius: Theme.controls.inputRadius
            }

            ColumnLayout {
                width: parent.width
                spacing: 10

                Label {
                    text: qsTr("Remove %1?").arg(root.emulator.name)
                    color: Theme.controls.labelColor
                    font.weight: Theme.font.strongWeight
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Its block is removed from emulators.toml, and it's taken off the platforms that use it.")
                          + (popup.pinned > 0
                             ? " " + (popup.pinned === 1 ? qsTr("1 game is pinned to it and will use its platform's emulators.")
                                                         : qsTr("%1 games are pinned to it and will use their platform's emulators.").arg(popup.pinned))
                             : "")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
                Label {
                    visible: popup.error !== ""
                    Layout.fillWidth: true
                    text: popup.error
                    color: Theme.controls.problemsText
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Theme.controls.controlSpacing
                    SettingsButton { text: qsTr("Cancel"); onClicked: popup.close() }
                    SettingsButton {
                        danger: true
                        text: qsTr("Remove")
                        onClicked: {
                            popup.error = Library.removeEmulator(root.emulator.id)
                            if (popup.error === "")
                                popup.close()
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: uninstallDialog
        active: false
        sourceComponent: MotionPopup {
            id: confirm

            readonly property bool flatpak: root.installInfo?.route === "flatpak"

            Component.onCompleted: open()
            onClosed: uninstallDialog.active = false

            parent: Overlay.overlay
            anchors.centerIn: parent
            modal: true
            Overlay.modal: Scrim {}
            padding: Theme.menu.dialogPadding
            width: 380
            background: Rectangle {
                color: Theme.menu.popupBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth
                radius: Theme.controls.inputRadius
            }

            ColumnLayout {
                width: parent.width
                spacing: 10

                Label {
                    text: qsTr("Uninstall %1?").arg(root.emulator.name)
                    color: Theme.controls.labelColor
                    font.weight: Theme.font.strongWeight
                }
                Label {
                    Layout.fillWidth: true
                    text: (confirm.flatpak ? qsTr("The Flatpak %1 is removed from your user installation.").arg(root.installInfo?.file ?? "")
                                           : qsTr("%1 goes to the trash.").arg((root.installInfo?.file ?? "").split("/").pop()))
                          + " " + qsTr("Your saves and keys stay, so installing it again picks up where you left off.")
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Theme.controls.controlSpacing
                    SettingsButton { text: qsTr("Cancel"); onClicked: confirm.close() }
                    SettingsButton {
                        danger: true
                        text: qsTr("Uninstall")
                        onClicked: {
                            Installer.uninstall(root.emulator.id)
                            confirm.close()
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: pathDialog
        active: false
        sourceComponent: ProgramDialog {
            title: qsTr("Choose %1's program").arg(root.emulator.name)
            onPicked: path => root.save("path", path)
            onClosed: pathDialog.active = false
        }
    }
}
