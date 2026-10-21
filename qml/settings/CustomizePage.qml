import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    readonly property var schema: Theme.customizeSchema
    readonly property string currentSection: list.currentSection
    readonly property string currentSubsection: list.currentSubsection

    readonly property string description:
        qsTr("Every color and style setting, by section. Changes show immediately.")

    readonly property var filters: {
        const out = []
        if (schema.show === "colors")
            out.push(qsTr("Colors"))
        else if (schema.show === "style")
            out.push(qsTr("Style"))
        if (schema.changedOnly)
            out.push(qsTr("Changed"))
        return out
    }

    signal message(string text, var link)

    function reveal(item, half) {
        if (half !== undefined && half !== "")
            schema.show = half
        if (item === "")
            return
        if (item === "theme" || item === "menu")
            themePopup.open()
        else if (item === "show")
            showPopup.open()
        else if (item === "session")
            sessionPopup.open()
        else if (item === "changed") {
            schema.show = "colors"
            schema.changedOnly = true
        }
        else if (item === "delete")
            askDelete(half === "style" ? "style" : "colors")
        else
            jumpTo(item)
    }

    function jumpTo(section) { list.jumpTo(section) }
    function jumpToSubsection(section, subsection) { list.jumpToSubsection(section, subsection) }
    function jumpBy(step) { list.jumpBy(step) }

    // A filter rebuilds the rows, which would drop the view to the top; stay on the section.
    function keepingSection(change) {
        const section = list.currentSection
        const subsection = list.currentSubsection
        return () => {
            change()
            let row = root.schema.firstRow(section, subsection)
            const inSubsection = row >= 0
            if (!inSubsection)
                row = root.schema.firstRow(section)
            if (row < 0)
                return
            list.sectionJump.jump(row, inSubsection ? Theme.settingsWindow.subsectionSpacing
                                                    : Theme.settingsWindow.sectionSpacing)
            // The rebuild moved currentSection to the top row; the jump itself is not tracked.
            list.currentSection = section
            list.currentSubsection = inSubsection ? subsection : list.itemAt(0, list.contentY + 1)?.subsection ?? ""
        }
    }

    function setShow(show) {
        list.sectionJump.swap(keepingSection(() => root.schema.show = show), 1)
    }

    function setChangedOnly(on) {
        list.sectionJump.swap(keepingSection(() => root.schema.changedOnly = on), on ? 1 : -1)
    }

    function showFollowers(section, key) {
        list.sectionJump.swap(keepingSection(() => root.schema.following = section + "." + key), 1)
    }

    function showSetting(section, key) {
        if (root.schema.rowOf(section, key) < 0) {
            search.clear()
            root.schema.changedOnly = false
            root.schema.show = "all"
        }
        const row = root.schema.rowOf(section, key)
        if (row >= 0)
            list.positionViewAtIndex(row, ListView.Center)
    }

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    function clearSearch() {
        if (search.text === "")
            return false
        search.clear()
        return true
    }

    function activeName(half) { return half === "colors" ? Theme.activeColors : Theme.activeStyle }

    function newItem(half) { newDialog.openFor(half, activeName(half)) }

    function openFile(half) {
        const err = GameRunner.openFile(half === "colors" ? Theme.colorsFilePath() : Theme.styleFilePath())
        if (err !== "")
            message(qsTr("Could not open — path: %1").arg(err), null)
    }

    function askReset(half) { resetDialog.openFor(half, activeName(half)) }
    function askDelete(half) { deleteDialog.openFor(half, activeName(half)) }
    function askRename(half) { renameDialog.openFor(half, activeName(half)) }
    function askDescription() { descriptionDialog.openFor(Theme.activeStyle) }

    // Dialogs open once the popover has started closing, so it doesn't sit over their scrim.
    function fromPopover(fn) {
        themePopup.close()
        Qt.callLater(fn)
    }

    Connections {
        target: root.Window.window
        function onVisibleChanged() {
            if (root.Window.window.visible) {
                root.schema.show = "all"
                root.schema.changedOnly = false
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
            spacing: 10

            SettingsSearchField {
                id: search
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.preferredWidth: 120
                placeholderText: qsTr("Search settings")

                Binding {
                    target: root.schema
                    property: "filterText"
                    value: search.text
                }
            }

            SettingsButton {
                id: sessionButton
                implicitHeight: Theme.settingsWindow.toolbarHeight
                Layout.minimumWidth: implicitWidth
                visible: Theme.sessionChanges.length > 0
                checkable: true
                checked: sessionPopup.opened
                primary: checked
                text: qsTr("This session (%1)").arg(Theme.sessionChanges.length)
                tip: qsTr("Settings changed since Settings opened")
                onClicked: sessionPopup.opened ? sessionPopup.close() : sessionPopup.open()

                SessionChangesPopup {
                    id: sessionPopup
                    x: sessionButton.width - width
                    y: sessionButton.height + Theme.controls.controlSpacing
                    onShowSetting: (section, key) => root.showSetting(section, key)
                }
            }

            SettingsButton {
                id: themeButton

                TextMetrics {
                    id: themeMetrics
                    font: themeLabel.font
                    text: themeLabel.text
                }

                implicitHeight: Theme.settingsWindow.toolbarHeight
                implicitWidth: Math.ceil(Math.min(themeMetrics.advanceWidth, 200)) + 1 + Theme.controls.iconSize + Theme.controls.controlSpacing + leftPadding + rightPadding
                Layout.preferredWidth: implicitWidth
                Layout.minimumWidth: 70
                leftPadding: 12
                rightPadding: 10
                tip: qsTr("Colors and style")
                onClicked: themePopup.opened ? themePopup.close() : themePopup.open()

                contentItem: RowLayout {
                    spacing: Theme.controls.controlSpacing

                    Label {
                        id: themeLabel
                        Layout.fillWidth: true
                        text: Theme.activeColors + " · " + Theme.activeStyle
                        color: themeButton.foreground
                        elide: Text.ElideRight
                    }
                    Icon {
                        path: Icons.chevron
                        size: Theme.controls.iconSize
                        color: Theme.controls.iconColor
                        rotation: themePopup.opened ? -90 : 90

                        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                    }
                }

                MotionPopup {
                    id: themePopup
                    readonly property real windowWidth: Overlay.overlay ? Overlay.overlay.width : root.width
                    readonly property real edgeGap: 8
                    readonly property real fitWidth: Math.min(Theme.settingsWindow.themePopupWidth,
                                                              windowWidth - pageLeft - 2 * edgeGap)
                    // The chip grows leftwards with its label, so its right edge is what stays put while the popup is open.
                    property real buttonRight: 0
                    readonly property real buttonLeft: buttonRight - themeButton.width
                    property real pageLeft: 0
                    x: Math.max(pageLeft + edgeGap - buttonLeft,
                                Math.min(themeButton.width - width,
                                         windowWidth - edgeGap - buttonLeft - width))
                    y: themeButton.height + Theme.controls.controlSpacing
                    width: fitWidth
                    padding: Theme.settingsWindow.themePopupPadding
                    // The button toggles it; a press on it must not close and reopen it.
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    onAboutToShow: { buttonRight = themeButton.mapToItem(null, themeButton.width, 0).x; pageLeft = root.mapToItem(null, 0, 0).x; colorsColumn.refresh(); styleColumn.refresh() }

                    dim: true
                    Overlay.modeless: Scrim { color: Theme.menu.popupScrim }

                    background: PopoverBackground {}

                    RowLayout {
                        width: themePopup.availableWidth
                        spacing: Theme.settingsWindow.themePopupColumnSpacing

                        ThemePickColumn {
                            id: colorsColumn
                            half: "colors"
                            Layout.alignment: Qt.AlignTop
                            Layout.fillWidth: true
                            Layout.preferredWidth: 100
                            onNewRequested: root.fromPopover(() => root.newItem("colors"))
                            onOpenFileRequested: root.openFile("colors")
                            onResetRequested: root.fromPopover(() => root.askReset("colors"))
                            onRenameRequested: root.fromPopover(() => root.askRename("colors"))
                            onDeleteRequested: root.fromPopover(() => root.askDelete("colors"))
                        }

                        Rectangle {
                            Layout.fillHeight: true
                            implicitWidth: 1
                            color: Theme.settingsWindow.rowDivider
                        }

                        ThemePickColumn {
                            id: styleColumn
                            half: "style"
                            Layout.alignment: Qt.AlignTop
                            Layout.fillWidth: true
                            Layout.preferredWidth: 100
                            onDescriptionRequested: root.fromPopover(() => root.askDescription())
                            onNewRequested: root.fromPopover(() => root.newItem("style"))
                            onOpenFileRequested: root.openFile("style")
                            onResetRequested: root.fromPopover(() => root.askReset("style"))
                            onRenameRequested: root.fromPopover(() => root.askRename("style"))
                            onDeleteRequested: root.fromPopover(() => root.askDelete("style"))
                        }
                    }
                }
            }

            SettingsButton {
                id: showButton
                implicitHeight: Theme.settingsWindow.toolbarHeight
                implicitWidth: showLabel.implicitWidth + Theme.controls.iconSize + Theme.controls.controlSpacing + leftPadding + rightPadding
                Layout.minimumWidth: implicitWidth
                primary: root.filters.length > 0
                leftPadding: 12
                rightPadding: 10
                tip: qsTr("Choose what the list shows")
                onClicked: showPopup.opened ? showPopup.close() : showPopup.open()

                contentItem: RowLayout {
                    spacing: Theme.controls.controlSpacing

                    Label {
                        id: showLabel
                        text: (root.filters.length > 0 ? root.filters.join(" · ") : qsTr("Show"))
                              + (root.schema.changedCount > 0 ? " (" + root.schema.changedCount + ")" : "")
                        color: showButton.foreground
                    }
                    Icon {
                        path: Icons.chevron
                        size: Theme.controls.iconSize
                        color: showButton.primary ? showButton.foreground : Theme.controls.iconColor
                        rotation: showPopup.opened ? -90 : 90

                        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                    }
                }

                MotionPopup {
                    id: showPopup
                    x: showButton.width - width
                    y: showButton.height + Theme.controls.controlSpacing
                    padding: 10
                    width: 260
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    dim: true
                    Overlay.modeless: Scrim { color: Theme.menu.popupScrim }

                    background: PopoverBackground {}

                    ColumnLayout {
                        width: parent.width
                        spacing: 10

                        CaptionLabel {
                            Layout.fillWidth: true
                            text: qsTr("Show")
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize + 1
                        }

                        SegmentedToggle {
                            Layout.fillWidth: true
                            accessibleName: qsTr("Show")
                            choices: [{ id: "all", text: qsTr("All") },
                                      { id: "colors", text: qsTr("Colors") },
                                      { id: "style", text: qsTr("Style") }]
                            current: root.schema.show
                            onPicked: id => root.setShow(id)
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.settingsWindow.rowDivider
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: qsTr("Hide unchanged settings")
                                    color: Theme.controls.descriptionColor
                                }
                            }
                            Label {
                                visible: root.schema.changedCount > 0
                                text: root.schema.changedCount
                                font.weight: Theme.font.emphasisWeight
                                color: Theme.controls.descriptionColor
                            }
                            SettingsSwitch {
                                checked: root.schema.changedOnly
                                onToggled: root.setChangedOnly(checked)
                            }
                        }
                    }
                }
            }

            ProblemsButton {
                problems: Theme.problems
            }
        }

        ThemeSettingsList {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            schema: root.schema
            emptyText: root.schema.changedOnly && search.text === ""
                       ? qsTr("Nothing changed yet.")
                       : qsTr("No settings match.")
            onMessage: (text, link) => root.message(text, link)
            onFollowersRequested: (section, key) => root.showFollowers(section, key)
        }
    }

    NewThemeDialog { id: newDialog }

    DeleteThemeDialog {
        id: deleteDialog
        onDeleted: text => root.message(text, null)
    }

    RenameThemeDialog {
        id: renameDialog
        onRenamed: text => root.message(text, null)
    }

    EditDescriptionDialog {
        id: descriptionDialog
        onSaved: text => root.message(text, null)
    }

    ResetThemeDialog {
        id: resetDialog
        onDone: text => root.message(text, null)
    }
}
