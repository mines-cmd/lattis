import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var problems: []
    readonly property var removable: problems.filter(p => Theme.unknownSetting(p).setting !== undefined)

    signal message(string text, var link)

    function open() { problemsPopup.open() }

    function removeUnknown(list) {
        const first = Theme.unknownSetting(list[0])
        const result = Theme.removeUnknownSettings(list)
        const undo = result.removed === 0 ? null : {
            text: qsTr("Undo"),
            isUndo: true,
            run: () => {
                const error = Theme.undoRemoveUnknownSettings()
                if (error !== "")
                    root.message(qsTr("Could not undo: %1").arg(error), null)
            }
        }
        if (result.error !== "")
            root.message(qsTr("Could not delete %1").arg(result.error), undo)
        else if (list.length === 1)
            root.message(qsTr("Deleted %1 from %2").arg(first.setting).arg(first.file), undo)
        else
            root.message(qsTr("Deleted %n unknown setting(s)", "", result.removed), undo)
    }

    onProblemsChanged: if (problems.length === 0) problemsPopup.close()

    visible: problems.length > 0
    Layout.preferredWidth: problemsLabel.implicitWidth + 20
    Layout.preferredHeight: Theme.settingsWindow.toolbarHeight
    radius: Theme.controls.inputRadius
    color: Theme.controls.problemsBackground

    Label {
        id: problemsLabel
        anchors.centerIn: parent
        text: qsTr("%n problem(s)", "", root.problems.length)
        color: Theme.controls.problemsText
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: problemsPopup.visible = !problemsPopup.visible
    }

    MotionPopup {
        id: problemsPopup
        y: parent.height + 4
        x: parent.width - width
        width: 420
        padding: Theme.layout.spacing
        background: PopoverBackground { fill: Theme.controls.problemsBackground }
        contentItem: ColumnLayout {
            spacing: Theme.layout.spacingSmall

            // Above the list, so a long one can't push it off the window.
            SettingsButton {
                visible: root.removable.length > 1
                Layout.alignment: Qt.AlignRight
                danger: true
                text: qsTr("Delete %n unknown setting(s)", "", root.removable.length)
                onClicked: root.removeUnknown(root.removable)
            }

            Repeater {
                model: root.problems
                RowLayout {
                    id: row
                    required property string modelData
                    readonly property string file: GameRunner.problemFile(modelData)
                    readonly property var unknown: Theme.unknownSetting(modelData)
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.fillWidth: true
                        text: row.modelData
                        color: Theme.controls.problemsText
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.controls.descriptionFontSize
                    }
                    IconButton {
                        visible: row.unknown.setting !== undefined
                        Layout.alignment: Qt.AlignVCenter
                        plain: true
                        implicitWidth: Theme.settingsWindow.problemsButtonSize
                        implicitHeight: Theme.settingsWindow.problemsButtonSize
                        iconSize: Theme.settingsWindow.problemsIconSize
                        iconColor: Theme.controls.problemsText
                        icon: Icons.deleteBin6Line
                        tip: qsTr("Delete %1 from %2").arg(row.unknown.setting ?? "").arg(row.unknown.file ?? "")
                        onClicked: root.removeUnknown([row.modelData])
                    }
                    IconButton {
                        visible: row.file !== ""
                        Layout.alignment: Qt.AlignVCenter
                        plain: true
                        implicitWidth: Theme.settingsWindow.problemsButtonSize
                        implicitHeight: Theme.settingsWindow.problemsButtonSize
                        iconSize: Theme.settingsWindow.problemsIconSize
                        iconColor: Theme.controls.problemsText
                        icon: Icons.openExternal
                        tip: qsTr("Open the file")
                        onClicked: GameRunner.openFile(row.file)
                    }
                }
            }
        }
    }
}
