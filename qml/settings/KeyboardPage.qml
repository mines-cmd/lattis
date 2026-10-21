import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Shortcuts for every action, in both windows. They're saved in settings.toml.")

    property string currentSection: Actions.model.sections.length > 0 ? Actions.model.sections[0].name : ""

    property string recordingName: ""

    onRecordingNameChanged: Actions.recording = recordingName !== ""
    onVisibleChanged: if (!visible) recordingName = ""

    function jumpTo(section) {
        const row = Actions.model.firstRow(section)
        if (row < 0)
            return
        currentSection = section
        sectionJump.go(row)
    }

    SectionJump {
        id: sectionJump
        list: list
    }

    function jumpBy(step) {
        const sections = Actions.model.sections
        const i = sections.findIndex(s => s.name === currentSection)
        const next = sections[i + step]
        if (next !== undefined)
            jumpTo(next.name)
    }

    signal message(string text)

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

    function updateCurrentSection() {
        if (sectionJump.busy)
            return
        const item = list.itemAt(0, list.contentY + 1)
        if (item)
            currentSection = item.section
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
                placeholderText: qsTr("Search shortcuts")

                Binding {
                    target: Actions.model
                    property: "filterText"
                    value: search.text
                }
            }

            SettingsButton {
                implicitHeight: Theme.settingsWindow.toolbarHeight
                leftPadding: (implicitHeight - Theme.controls.iconSize) / 2
                rightPadding: leftPadding
                tip: qsTr("Open settings.toml")
                iconPath: Icons.openExternal
                onClicked: {
                    const err = GameRunner.openFile(Actions.settingsFile())
                    if (err !== "")
                        message(qsTr("Could not open — path: %1").arg(err))
                }
            }

            ProblemsButton {
                problems: Actions.problems
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            model: Actions.model
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            Label {
                visible: list.count === 0
                anchors.horizontalCenter: parent.horizontalCenter
                y: 40
                text: qsTr("No shortcuts match.")
                color: Theme.controls.descriptionColor
            }

            onContentYChanged: root.updateCurrentSection()
            onCountChanged: root.updateCurrentSection()

            delegate: ColumnLayout {
                readonly property string section: model.group

                width: list.width - scrollBar.width - 8
                spacing: 0

                Label {
                    visible: model.firstInGroup
                    Layout.topMargin: index === 0 ? 0 : Theme.settingsWindow.sectionSpacing
                    Layout.bottomMargin: 10
                    text: model.groupTitle
                    color: Theme.settingsWindow.sectionHeaderColor
                    font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
                    font.weight: Theme.font.strongWeight
                }

                Item {
                    id: slice

                    readonly property real padding: Theme.settingsWindow.cardPadding
                    readonly property real topPadding: model.firstInGroup ? padding : 0
                    readonly property real bottomPadding: model.lastInGroup ? padding : 0

                    Layout.fillWidth: true
                    implicitHeight: shortcutRow.implicitHeight + topPadding + bottomPadding
                    clip: true

                    Rectangle {
                        readonly property real overhang: radius + border.width + 1

                        y: model.firstInGroup ? 0 : -overhang
                        width: parent.width
                        height: parent.height + (model.firstInGroup ? 0 : overhang) + (model.lastInGroup ? 0 : overhang)
                        radius: Theme.settingsWindow.cardRadius
                        color: Theme.settingsWindow.cardBackground
                        border.color: Theme.settingsWindow.cardBorder
                        border.width: Theme.settingsWindow.cardBorderWidth
                    }

                    Rectangle {
                        visible: Theme.settingsWindow.rowDividers && !model.firstInGroup
                        x: slice.padding + Theme.settingsWindow.rowPadding
                        width: parent.width - x * 2
                        height: 1
                        color: Theme.settingsWindow.rowDivider
                    }

                    ShortcutRow {
                        id: shortcutRow
                        firstInCard: model.firstInGroup
                        lastInCard: model.lastInGroup
                        x: slice.padding
                        y: slice.topPadding
                        width: parent.width - slice.padding * 2
                        name: model.name
                        title: model.title
                        keyLabels: model.keyLabels
                        defaultLabels: model.defaultLabels
                        isCustom: model.isCustom
                        conflict: model.conflict
                        recording: root.recordingName === model.name
                        onRecordRequested: root.recordingName = model.name
                        onRecordFinished: if (root.recordingName === model.name) root.recordingName = ""
                    }
                }
            }
        }
    }
}
