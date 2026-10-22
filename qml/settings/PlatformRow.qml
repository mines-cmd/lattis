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
    property int position: 0
    property int count: 0
    // Set right after this platform moved by typing its place: the box takes focus again.
    property bool focusPosition: false
    property string error: ""

    readonly property bool editable: platform.defined
    readonly property Item handle: grip.area

    property bool draggable: true

    signal dragStarted(var handle, real x, real y)
    signal dragMoved(var handle, real x, real y)
    signal dragEnded(bool dropped)
    signal toggleRequested()
    signal moveToRequested(int position)
    signal removeRequested()
    signal defineRequested()

    spacing: 0

    function save(key, value) {
        root.error = Library.setPlatformValue(root.platform.id, key, value)
    }

    function emulatorInfo(id) {
        return Library.emulators.find(e => e.id === id)
    }

    function edited(list, i, step) {
        const out = list.slice()
        if (step === 0) {
            out.splice(i, 1)
        } else {
            const j = i + step
            if (j < 0 || j >= out.length)
                return out
            out[i] = list[j]
            out[j] = list[i]
        }
        return out
    }

    property bool showAllEmulators: false
    readonly property var addableEmulators: {
        const unused = Library.emulators.filter(e => !root.platform.emulators.includes(e.id))
        const suggested = unused.filter(e => e.platformIds.includes(root.platform.id))
        return showAllEmulators ? suggested.concat(unused.filter(e => !suggested.includes(e))) : suggested
    }

    Component.onCompleted: {
        if (focusPosition) {
            positionField.forceActiveFocus()
            positionField.selectAll()
        }
    }

    Rectangle {
        id: header
        Layout.fillWidth: true
        implicitHeight: headerRow.implicitHeight + Theme.settingsWindow.rowPadding * 2
        radius: Theme.settingsWindow.rowRadius
        // Corners against the card's follow its curve, so the gap around them is even.
        topLeftRadius: root.firstInCard ? root.cardCornerRadius : radius
        topRightRadius: topLeftRadius
        bottomLeftRadius: root.lastInCard && details.progress === 0 && !problemList.visible ? root.cardCornerRadius : radius
        bottomRightRadius: bottomLeftRadius
        color: headerHover.hovered || header.activeFocus ? Theme.controls.hover : "transparent"
        activeFocusOnTab: root.editable

        Keys.onReturnPressed: root.toggleRequested()
        Keys.onEnterPressed: root.toggleRequested()
        Keys.onSpacePressed: root.toggleRequested()

        HoverHandler { id: headerHover; cursorShape: root.editable ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler {
            enabled: root.editable
            onTapped: { header.forceActiveFocus(); root.toggleRequested() }
        }

        RowLayout {
            id: headerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.settingsWindow.rowPadding
            spacing: 10

            DragGrip {
                id: grip
                opacity: root.editable ? (root.draggable ? 1 : 0.4) : 0
                draggable: root.editable && root.draggable
                tip: !root.editable ? "" : root.draggable ? qsTr("Drag to change its place in the rail")
                                                          : qsTr("Clear the search to drag platforms")
                onDragStarted: (handle, x, y) => root.dragStarted(handle, x, y)
                onDragMoved: (handle, x, y) => root.dragMoved(handle, x, y)
                onDragEnded: dropped => root.dragEnded(dropped)
            }

            Icon {
                path: Icons.chevron
                size: Theme.controls.iconSize
                color: Theme.controls.iconColor
                rotation: root.expanded ? 90 : 0
                opacity: root.editable ? 1 : 0

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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: !inlineProblems.visible
                        text: root.platform.name
                        color: Theme.controls.labelColor
                        font.pixelSize: Theme.controls.labelFontSize
                        elide: Text.ElideRight

                        HoverHandler { id: nameHover }
                        ToolTip.visible: nameHover.hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("Game files refer to it as “%1”").arg(root.platform.id)
                    }
                    Label {
                        id: inlineProblems
                        visible: text !== ""
                        Layout.fillWidth: true
                        text: root.platform.problems.filter(p => p.command === "").map(p => p.text).join(" · ")
                        color: Theme.controls.problemsText
                        font.pixelSize: Theme.controls.descriptionFontSize
                        elide: Text.ElideRight

                        HoverHandler { id: problemsHover }
                        ToolTip.visible: problemsHover.hovered && truncated
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: text
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: {
                        const games = root.platform.games
                        const parts = [games === 1 ? qsTr("1 game") : qsTr("%1 games").arg(games)]
                        if (!root.editable)
                            parts.push(qsTr("not in platforms.toml"))
                        else if (root.platform.emulatorNames.length > 0)
                            parts.push(root.platform.emulatorNames.join(", "))
                        return parts.join(" · ")
                    }
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    elide: Text.ElideRight
                }
            }

            SettingsButton {
                visible: !root.editable
                text: qsTr("Add to platforms.toml")
                tip: qsTr("Define this platform, so you can name it and give it emulators and ROM folders")
                onClicked: root.defineRequested()
            }

            SettingsTextField {
                id: positionField
                visible: root.editable
                Layout.preferredWidth: Theme.settingsWindow.toolbarHeight + 12
                leftPadding: 16
                rightPadding: 2
                implicitHeight: Theme.controls.inputHeight
                horizontalAlignment: TextInput.AlignHCenter
                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                text: root.position
                ToolTip.visible: hovered && !activeFocus
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Place in the rail: type another to move it there")

                Text {
                    anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                    text: "#"
                    font: positionField.font
                    color: Theme.controls.placeholderColor
                }

                function apply() {
                    const to = Math.max(1, Math.min(root.count, parseInt(text)))
                    if (isNaN(to) || to === root.position)
                        text = root.position
                    else
                        root.moveToRequested(to)
                }
                onEditingFinished: apply()
                // Handled here, so Enter doesn't reach the row, which it would expand.
                Keys.onReturnPressed: apply()
                Keys.onEnterPressed: apply()
                Keys.onEscapePressed: {
                    text = root.position
                    header.forceActiveFocus()
                }
            }
        }
    }

    ColumnLayout {
        id: problemList
        readonly property var withCommand: root.platform.problems.filter(p => p.command !== "")
        visible: withCommand.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: Theme.settingsWindow.rowPadding * 2 + Theme.controls.iconSize
        Layout.rightMargin: Theme.settingsWindow.rowPadding
        Layout.bottomMargin: Theme.settingsWindow.rowPadding
        spacing: 6

        Repeater {
            model: problemList.withCommand

            ColumnLayout {
                id: problem
                required property var modelData

                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: problem.modelData.text
                    color: Theme.controls.problemsText
                    font.pixelSize: Theme.controls.descriptionFontSize
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    visible: problem.modelData.command !== ""
                    Layout.fillWidth: true
                    spacing: Theme.controls.controlSpacing

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: commandText.implicitHeight + 12
                        radius: Theme.controls.inputRadius
                        color: Theme.controls.inputBackground
                        border.color: Theme.controls.inputBorder
                        border.width: Theme.controls.inputBorderWidth

                        TextEdit {
                            id: commandText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 8
                            text: problem.modelData.command
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            color: Theme.controls.inputText
                            selectionColor: Theme.controls.inputBorderFocus
                            font.pixelSize: Theme.controls.descriptionFontSize
                        }
                    }
                    SettingsButton {
                        text: qsTr("Copy")
                        tip: qsTr("Copy the command, to run it in a terminal")
                        onClicked: {
                            commandText.selectAll()
                            commandText.copy()
                            commandText.deselect()
                        }
                    }
                }
            }
        }
    }

    Unfold {
        id: details
        open: root.expanded && root.editable
        Layout.fillWidth: true
        leftPadding: Theme.settingsWindow.rowPadding * 2 + Theme.controls.iconSize
        rightPadding: Theme.settingsWindow.rowPadding
        topPadding: Theme.settingsWindow.rowPadding * 1.5
        bottomPadding: Theme.settingsWindow.rowPadding * 1.5
        spacing: Theme.settingsWindow.rowPadding * 1.5

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true

                SectionTitle {
                    Layout.fillWidth: true
                    text: qsTr("Name and icon")
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
                    ToolTip.text: qsTr("Game files refer to it as \"%1\", which stays when you rename it.").arg(root.platform.id)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                SettingsTextField {
                    id: nameField
                    Layout.fillWidth: true
                    implicitHeight: Theme.controls.inputHeight
                    text: root.platform.name
                    onEditingFinished: if (text.trim() !== root.platform.name) root.save("name", text)
                }
                SettingsButton {
                    id: iconButton
                    iconPath: Icons.forPlatform(root.platform.icon)
                    text: "▾"
                    tip: qsTr("Icon: %1").arg(Icons.platformIconLabel(root.platform.icon))
                    onClicked: iconPopup.open()

                    MotionPopup {
                        id: iconPopup
                        y: iconButton.height + 4
                        x: iconButton.width - width
                        padding: 6
                        background: Rectangle {
                            radius: Theme.controls.inputRadius
                            color: Theme.menu.popupBackground
                            border.color: Theme.controls.inputBorder
                            border.width: Math.max(1, Theme.controls.inputBorderWidth)
                        }

                        Grid {
                            columns: 6
                            spacing: 6

                            Repeater {
                                model: Icons.platformIconNames

                                SettingsButton {
                                    required property string modelData

                                    leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                                    rightPadding: leftPadding
                                    iconPath: Icons.forPlatform(modelData)
                                    primary: root.platform.icon === modelData
                                    tip: Icons.platformIconLabel(modelData)
                                    onClicked: {
                                        iconPopup.close()
                                        if (!primary)
                                            root.save("icon", modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= Theme.settingsWindow.platformColumnsMinWidth ? 2 : 1
            columnSpacing: Theme.settingsWindow.rowPadding * 2
            rowSpacing: Theme.settingsWindow.rowPadding * 1.5

            Section {
                title: qsTr("Emulators")
                actions: ListAction {
                    id: addEmulator
                    text: qsTr("Add emulator")
                    onClicked: openMenu()
                    function openMenu() {
                        emulatorMenu.popup(addEmulator, addEmulator.width - emulatorMenu.width, addEmulator.height)
                    }

                    SettingsMenu {
                        id: emulatorMenu

                        Instantiator {
                            model: root.addableEmulators.length > 0
                                   ? root.addableEmulators
                                   : [{ id: "", name: qsTr("No other emulators are made for %1").arg(root.platform.name) }]
                            delegate: Action {
                                required property var modelData
                                text: modelData.name
                                enabled: modelData.id !== ""
                                onTriggered: root.save("emulators", root.platform.emulators.concat([modelData.id]))
                            }
                            onObjectAdded: (index, object) => emulatorMenu.insertAction(index, object)
                            onObjectRemoved: (index, object) => emulatorMenu.removeAction(object)
                        }
                        MenuSeparator {
                            contentItem: Rectangle {
                                implicitHeight: 1
                                color: Theme.settingsWindow.rowDivider
                            }
                        }
                        Action {
                            text: root.showAllEmulators ? qsTr("Only those made for %1").arg(root.platform.name)
                                                        : qsTr("Show those made for other platforms")
                            onTriggered: {
                                root.showAllEmulators = !root.showAllEmulators
                                Qt.callLater(addEmulator.openMenu)
                            }
                        }
                    }
                }

                Repeater {
                    model: root.platform.emulators

                    ListRow {
                        id: emulatorItem
                        required property string modelData
                        required property int index
                        readonly property var info: root.emulatorInfo(modelData)
                        last: index === root.platform.emulators.length - 1

                        Label {
                            text: emulatorItem.index + 1
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                        }
                        Label {
                            text: emulatorItem.info ? emulatorItem.info.name : emulatorItem.modelData
                            color: Theme.controls.inputText
                        }
                        Label {
                            Layout.fillWidth: true
                            text: !emulatorItem.info ? qsTr("unknown emulator id; see emulators.reference.toml")
                                : emulatorItem.info.detecting ? ""
                                : !emulatorItem.info.found ? qsTr("not installed")
                                : emulatorItem.index === 0 ? qsTr("default") : ""
                            color: emulatorItem.info && emulatorItem.info.found ? Theme.controls.descriptionColor
                                                                                 : Theme.settingsWindow.installMissing
                            font.pixelSize: Theme.controls.descriptionFontSize
                            elide: Text.ElideRight
                        }
                        ListButton {
                            visible: root.platform.emulators.length > 1
                            enabled: emulatorItem.index > 0
                            iconPath: Icons.chevron
                            iconRotation: -90
                            tip: qsTr("Prefer it over the one above")
                            onClicked: root.save("emulators", root.edited(root.platform.emulators, emulatorItem.index, -1))
                        }
                        ListButton {
                            visible: root.platform.emulators.length > 1
                            enabled: emulatorItem.index < root.platform.emulators.length - 1
                            iconPath: Icons.chevron
                            iconRotation: 90
                            tip: qsTr("Prefer the one below over it")
                            onClicked: root.save("emulators", root.edited(root.platform.emulators, emulatorItem.index, 1))
                        }
                        ListButton {
                            iconPath: Icons.close
                            tip: qsTr("Remove from this platform")
                            onClicked: root.save("emulators", root.edited(root.platform.emulators, emulatorItem.index, 0))
                        }
                    }
                }
                ListRow {
                    visible: root.platform.emulators.length === 0
                    last: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("None: this platform's games don't launch in an emulator.")
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                        wrapMode: Text.WordWrap
                    }
                }
                note: root.platform.emulators.length > 1 ? qsTr("A game launches in the first one that's installed.") : ""
            }

            Section {
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
                            tip: qsTr("Stop scanning this folder; its games stay, as \"ROM not found\"")
                            onClicked: root.save("rom_folders", root.edited(root.platform.romFolders, folderItem.index, 0))
                        }
                    }
                }
                ListRow {
                    visible: root.platform.romFolders.length === 0
                    last: true

                    Label {
                        Layout.fillWidth: true
                        text: root.platform.emulators.length > 0 ? qsTr("None yet: add the folder this platform's ROMs are in.")
                                                                 : qsTr("None.")
                        color: Theme.controls.descriptionColor
                        font.pixelSize: Theme.controls.descriptionFontSize
                        wrapMode: Text.WordWrap
                    }
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

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.controls.controlSpacing

            SettingsCheckBox {
                Layout.fillWidth: true
                text: qsTr("Show in the rail")
                checked: root.platform.inRail
                onToggled: root.save("in_rail", checked)
            }
            SettingsButton {
                danger: true
                text: qsTr("Remove platform")
                tip: qsTr("Its games stay in the library")
                onClicked: root.removeRequested()
            }
        }
    }

    Loader {
        id: folderDialog
        active: false
        sourceComponent: RomFolderDialog {
            title: qsTr("Add a ROM folder to %1").arg(root.platform.name)
            onPicked: path => {
                if (!root.platform.romFolders.includes(path))
                    root.save("rom_folders", root.platform.romFolders.concat([path]))
            }
            onClosed: folderDialog.active = false
        }
    }
}
