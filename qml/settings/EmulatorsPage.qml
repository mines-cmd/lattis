import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Each emulator is found as a Flatpak, a program on PATH, or an AppImage in ~/Applications or ~/AppImages, unless you choose its program.")

    readonly property string query: search.text.trim().toLowerCase()
    readonly property var shown: Library.emulators.filter(
        e => query === "" || [e.name, e.id, e.platforms.join(" ")].some(t => t.toLowerCase().includes(query)))
        .sort((a, b) => b.found - a.found)
    readonly property var presets: shown.filter(e => e.preset)
    readonly property var own: shown.filter(e => !e.preset)
    readonly property var problems: Library.problems.filter(p => p.includes("emulators.toml"))
    property string expandedId: ""

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
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

    function reveal(id) {
        expandedId = id
    }

    property Item rowToShow: null
    function showRow(item) {
        rowToShow = item
        Qt.callLater(scrollToRow)
    }
    function scrollToRow() {
        if (!rowToShow)
            return
        const top = rowToShow.mapToItem(flick.contentItem, 0, 0).y
        const bottom = top + rowToShow.height
        if (top < flick.contentY)
            flick.contentY = top
        else if (bottom > flick.contentY + flick.height)
            flick.contentY = Math.min(top, bottom - flick.height)
    }
    Connections {
        target: root.rowToShow
        function onHeightChanged() { Qt.callLater(root.scrollToRow) }
    }

    signal message(string text)

    function openFile() {
        const err = GameRunner.openFile(Library.emulatorsFile())
        if (err !== "")
            message(qsTr("Could not open — path: %1").arg(err))
    }

    component Card: Rectangle {
        id: card

        property var emulators: []

        Layout.fillWidth: true
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
                model: card.emulators

                ColumnLayout {
                    id: entry
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: 0

                    Rectangle {
                        visible: Theme.settingsWindow.rowDividers && entry.index > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.settingsWindow.rowPadding
                        Layout.rightMargin: Theme.settingsWindow.rowPadding
                        implicitHeight: 1
                        color: Theme.settingsWindow.rowDivider
                    }

                    EmulatorRow {
                        Layout.fillWidth: true
                        emulator: entry.modelData
                        expanded: root.expandedId === entry.modelData.id
                        firstInCard: entry.index === 0
                        lastInCard: entry.index === card.emulators.length - 1
                        onToggleRequested: root.expandedId = expanded ? "" : entry.modelData.id
                        onExpandedChanged: if (expanded) root.showRow(this)
                        onOpenFileRequested: root.openFile()
                    }
                }
            }
        }
    }

    component DialogBackground: Rectangle {
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth
        radius: Theme.settingsWindow.inputRadius
    }

    MotionPopup {
        id: addDialog

        readonly property string id: Library.emulatorIdFor(nameField.text)
        readonly property bool taken: Library.emulators.some(e => e.id === id)
        readonly property bool canAdd: nameField.text.trim() !== "" && id !== "" && !taken
        readonly property var starts: [{ name: qsTr("Blank"), id: "" }].concat(
            Library.emulators.filter(e => e.preset).map(e => ({ name: e.name, id: e.id }))
                .sort((a, b) => a.name.localeCompare(b.name)))

        function openFresh() {
            nameField.text = ""
            startFrom.currentIndex = 0
            addError.text = ""
            open()
            nameField.forceActiveFocus()
        }
        function add() {
            if (!canAdd)
                return
            const newId = id
            addError.text = Library.addEmulator(nameField.text, starts[startFrom.currentIndex].id)
            if (addError.text === "") {
                root.expandedId = newId
                close()
            }
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.settingsWindow.dialogPadding
        width: 380
        background: DialogBackground {}

        ColumnLayout {
            width: parent.width
            spacing: 10

            Label { text: qsTr("Add emulator"); color: Theme.settingsWindow.labelColor; font.bold: true }

            Label { text: qsTr("Start from"); color: Theme.settingsWindow.descriptionColor }
            SettingsComboBox {
                id: startFrom
                Layout.fillWidth: true
                model: addDialog.starts
                textRole: "name"
            }
            Label {
                Layout.fillWidth: true
                text: startFrom.currentIndex === 0 ? qsTr("A blank emulator has no platforms, ROM extensions or program yet; you fill them in next.")
                                                   : qsTr("Copies its platforms, ROM extensions and command, but not where it is installed or its saves, which a fork keeps elsewhere.")
                color: Theme.settingsWindow.descriptionColor
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            Label { text: qsTr("Name"); color: Theme.settingsWindow.descriptionColor }
            SettingsTextField {
                id: nameField
                Layout.fillWidth: true
                implicitHeight: Theme.settingsWindow.inputHeight
                placeholderText: qsTr("e.g. Citra")
                onAccepted: addDialog.add()
            }

            Label {
                Layout.fillWidth: true
                text: nameField.text.trim() === "" ? qsTr("The emulator's id is made from its name, and can't change later; the name can.")
                    : addDialog.id === "" ? qsTr("The id is made from the letters a-z and digits in the name; this one has none.")
                    : addDialog.taken ? qsTr("There's already an emulator with the id %1.").arg(addDialog.id)
                    : qsTr("Id: %1. It can't change later; the name can.").arg(addDialog.id)
                color: addDialog.canAdd || nameField.text.trim() === "" ? Theme.settingsWindow.descriptionColor
                                                                        : Theme.settingsWindow.problemsText
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            Label {
                id: addError
                visible: text !== ""
                Layout.fillWidth: true
                color: Theme.settingsWindow.problemsText
                font.pixelSize: Theme.settingsWindow.descriptionFontSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.settingsWindow.controlSpacing
                SettingsButton { text: qsTr("Cancel"); onClicked: addDialog.close() }
                SettingsButton {
                    primary: true
                    text: qsTr("Add")
                    enabled: addDialog.canAdd
                    onClicked: addDialog.add()
                }
            }
        }
    }

    component Heading: Label {
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.bold: true
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
                placeholderText: qsTr("Search emulators")
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                text: qsTr("Add emulator")
                iconPath: Icons.add
                tip: qsTr("Define an emulator of your own")
                onClicked: addDialog.openFresh()
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                text: qsTr("Detect again")
                tip: qsTr("Look for installed emulators and scan the ROM folders")
                onClicked: Library.scanNow()
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                leftPadding: (implicitHeight - Theme.settingsWindow.iconSize) / 2
                rightPadding: leftPadding
                tip: qsTr("Open emulators.toml")
                iconPath: Icons.openExternal
                onClicked: root.openFile()
            }

            ProblemsButton {
                problems: root.problems
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - scrollBar.width - 8
                spacing: 0

                Label {
                    visible: root.shown.length === 0
                    Layout.fillWidth: true
                    text: qsTr("No emulators match \"%1\".").arg(search.text.trim())
                    color: Theme.settingsWindow.descriptionColor
                    wrapMode: Text.WordWrap
                }

                Heading {
                    visible: root.presets.length > 0
                    Layout.bottomMargin: 10
                    text: qsTr("Built in")
                }

                Card {
                    visible: root.presets.length > 0
                    emulators: root.presets
                }

                Heading {
                    visible: root.query === "" || root.own.length > 0
                    Layout.topMargin: root.presets.length > 0 ? Theme.settingsWindow.sectionSpacing : 0
                    Layout.bottomMargin: 10
                    text: qsTr("Yours")
                }

                Card {
                    visible: root.own.length > 0
                    emulators: root.own
                }

                RowLayout {
                    visible: root.query === "" && root.own.length === 0
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Add an emulator Lattis doesn't know, or a fork of a built-in one. emulators.reference.toml, next to emulators.toml, shows every key with the built-in ones as examples.")
                        color: Theme.settingsWindow.descriptionColor
                        font.pixelSize: Theme.settingsWindow.descriptionFontSize
                        wrapMode: Text.WordWrap
                    }
                    SettingsButton {
                        text: qsTr("Add emulator")
                        iconPath: Icons.add
                        onClicked: addDialog.openFresh()
                    }
                }
            }
        }
    }
}
