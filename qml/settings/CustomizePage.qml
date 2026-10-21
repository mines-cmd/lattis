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

    // One entry per active filter: the banner shows `text`, the Show button `label`; `clear` drops it.
    readonly property var filters: {
        const out = []
        if (schema.show === "colors")
            out.push({ text: qsTr("Colors"), label: qsTr("Colors"), clear: () => setShow("all") })
        else if (schema.show === "style")
            out.push({ text: qsTr("Style"), label: qsTr("Style"), clear: () => setShow("all") })
        if (schema.sharedOnly)
            out.push({ text: qsTr("Shared values"), label: qsTr("Shared"), clear: () => setSharedOnly(false) })
        if (schema.following !== "")
            out.push({ text: qsTr("Following %1").arg(schema.followingTitle), label: qsTr("Following"),
                       clear: () => setFollowing("") })
        else if (schema.group !== "everything") {
            const title = schema.groups.find(g => g.name === schema.group)?.title ?? schema.group
            out.push({ text: title, label: title, clear: () => setGroup("everything") })
        }
        if (schema.changedOnly)
            out.push({ text: qsTr("Changed"), label: qsTr("Changed"), clear: () => setChangedOnly(false) })
        return out
    }

    signal message(string text, var link)

    // Screenshot hook: the list has built the row by the time this fires.
    Timer {
        id: openRowMenu
        property string target
        interval: 600
        onTriggered: {
            const row = root.schema.rowOf(target.slice(0, target.indexOf(".")), target.slice(target.indexOf(".") + 1))
            list.positionViewAtIndex(row, ListView.Center)
            list.itemAtIndex(row)?.rowItem.openMoreMenu()
        }
    }

    function reveal(item, half) {
        if (half !== undefined && half !== "")
            schema.show = half
        if (item === "")
            return
        if (item === "theme" || item === "menu")
            themePopup.open()
        else if (item === "show")
            showPopup.open()
        else if (item.startsWith("row:")) {
            const target = item.slice(4)
            const dot = target.indexOf(".")
            showSetting(target.slice(0, dot), target.slice(dot + 1))
            openRowMenu.target = target
            openRowMenu.restart()
        }
        else if (item.startsWith("group:"))
            schema.group = item.slice(6)
        else if (item.startsWith("following:"))
            schema.following = item.slice(10)
        else if (item === "shared")
            schema.sharedOnly = true
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

    function setSharedOnly(on) {
        list.sectionJump.swap(keepingSection(() => root.schema.sharedOnly = on), on ? 1 : -1)
    }

    function setGroup(group) {
        list.sectionJump.swap(keepingSection(() => root.schema.group = group), group === "everything" ? -1 : 1)
    }

    function setFollowing(target) {
        list.sectionJump.swap(keepingSection(() => root.schema.following = target), target === "" ? -1 : 1)
    }

    function showFollowers(section, key) { setFollowing(section + "." + key) }

    function clearFilters() {
        list.sectionJump.swap(keepingSection(() => {
            root.schema.show = "all"
            root.schema.sharedOnly = false
            root.schema.group = "everything"
            root.schema.following = ""
            root.schema.changedOnly = false
        }), -1)
    }

    function showSetting(section, key) {
        if (root.schema.rowOf(section, key) < 0) {
            search.clear()
            root.schema.changedOnly = false
            root.schema.show = "all"
            root.schema.sharedOnly = false
            root.schema.group = "everything"
            root.schema.following = ""
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
                root.schema.sharedOnly = false
                root.schema.group = "everything"
                root.schema.following = ""
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
                        text: (root.filters.length > 0 ? root.filters.map(f => f.label).join(" · ") : qsTr("Show"))
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

                        CaptionLabel {
                            Layout.fillWidth: true
                            text: qsTr("Group")
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize + 1
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.settingsWindow.themePopupRowSpacing

                            Repeater {
                                model: root.schema.groups

                                delegate: Rectangle {
                                    id: groupRow
                                    required property var modelData
                                    readonly property bool picked: root.schema.following === ""
                                                                   && root.schema.group === modelData.name

                                    Layout.fillWidth: true
                                    implicitHeight: groupLabel.implicitHeight + 8
                                    radius: Theme.settingsWindow.themePopupRowRadius
                                    color: picked ? Theme.settingsWindow.themePopupSelectedBackground : "transparent"

                                    Label {
                                        id: groupLabel
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: groupRow.modelData.title
                                        color: Theme.controls.descriptionColor
                                        font.weight: groupRow.picked ? Theme.font.emphasisWeight : Font.Normal
                                    }
                                    Label {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: groupRow.modelData.count
                                        color: Theme.controls.descriptionColor
                                        opacity: 0.7
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.setGroup(groupRow.modelData.name)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.settingsWindow.rowDivider
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: qsTr("Shared values only")
                                color: Theme.controls.descriptionColor
                            }
                            SettingsSwitch {
                                checked: root.schema.sharedOnly
                                onToggled: root.setSharedOnly(checked)
                            }
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

        Rectangle {
            Layout.fillWidth: true
            visible: root.filters.length > 0
            implicitHeight: Math.max(Theme.settingsWindow.filterBannerHeight, bannerRow.implicitHeight)
            radius: Theme.settingsWindow.filterBannerRadius
            color: Qt.alpha(Theme.settingsWindow.filterBannerColor, Theme.settingsWindow.filterBannerFillOpacity)
            border.color: Theme.settingsWindow.filterBannerColor
            border.width: Theme.settingsWindow.filterBannerBorderWidth

            RowLayout {
                id: bannerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.settingsWindow.filterBannerPadding
                anchors.rightMargin: Theme.settingsWindow.filterBannerPadding
                spacing: Theme.settingsWindow.filterBannerSpacing

                Icon {
                    Layout.alignment: Qt.AlignVCenter
                    size: Theme.controls.iconSize
                    path: root.schema.following !== "" ? Icons.link : Icons.filter
                    color: Theme.settingsWindow.filterBannerColor
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.settingsWindow.filterBannerSpacing

                    Repeater {
                        model: root.filters

                        delegate: Rectangle {
                            id: chip
                            required property var modelData

                            implicitWidth: chipLabel.implicitWidth + chipClose.width + 12 + Theme.settingsWindow.sourceTagPadding
                            implicitHeight: chipLabel.implicitHeight + 4
                            radius: height / 2
                            color: Qt.alpha(Theme.settingsWindow.filterBannerColor, Theme.settingsWindow.filterBannerChipOpacity)

                            Label {
                                id: chipLabel
                                x: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: chip.modelData.text
                                color: Theme.settingsWindow.filterBannerTextColor
                                font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                            }
                            Icon {
                                id: chipClose
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                size: Theme.settingsWindow.sourceTagFontSize + 2
                                path: Icons.close
                                color: Theme.settingsWindow.filterBannerTextColor
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: chip.modelData.clear()
                            }
                        }
                    }

                    Label {
                        height: Math.ceil(chipMetrics.height) + 4
                        verticalAlignment: Text.AlignVCenter
                        text: list.count === 1 ? qsTr("1 setting") : qsTr("%1 settings").arg(list.count)
                        color: Theme.settingsWindow.filterBannerColor

                        TextMetrics {
                            id: chipMetrics
                            font.pixelSize: Theme.settingsWindow.sourceTagFontSize
                            text: "0"
                        }
                    }
                }

                Label {
                    text: qsTr("Clear all")
                    color: Theme.settingsWindow.filterBannerColor
                    font.weight: Theme.font.emphasisWeight

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.clearFilters()
                    }
                }
            }
        }

        ThemeSettingsList {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            schema: root.schema
            emptyText: root.filters.length === 1 && root.schema.changedOnly && search.text === ""
                       ? qsTr("Nothing changed yet.")
                       : qsTr("No settings match.")
            onMessage: (text, link) => root.message(text, link)
            onFollowersRequested: (section, key) => root.showFollowers(section, key)
            onGoToRequested: (section, key) => root.showSetting(section, key)
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
