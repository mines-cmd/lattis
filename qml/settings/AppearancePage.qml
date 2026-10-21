import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    readonly property string description: qsTr("Pick a theme, or mix colors and a style. Every setting is on the Colors and Style pages.")

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    signal message(string text, var link)

    function saveTheme() { saveDialog.openFresh() }

    property string hoverTheme
    function reveal(item) {
        const [what, theme] = item.split(":")
        if (what === "hover")
            hoverTheme = theme
        else if (what === "delete")
            deleteDialog.openFor("theme", theme)
    }

    component LabeledRow: RowLayout {
        property alias label: name.text
        property alias note: noteLabel.text

        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                id: name
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                wrapMode: Text.WordWrap
            }
            SettingsNote {
                id: noteLabel
                Layout.fillWidth: true
                visible: text !== ""
            }
        }
    }

    component SectionHeader: Label {
        Layout.topMargin: Theme.settingsWindow.sectionSpacing / 2
        Layout.bottomMargin: Theme.layout.spacing
        color: Theme.settingsWindow.sectionHeaderColor
        font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
        font.bold: true
    }

    component ThemeCard: AbstractButton {
        id: card

        required property string modelData
        readonly property bool active: modelData === Theme.activeTheme
        readonly property var c: {
            void Theme.availableThemes
            const pair = Theme.themePair(modelData)
            return Theme.preview(pair.colors ?? "", pair.style ?? "")
        }
        readonly property real corner: Math.min(c.radius?.medium ?? 0, 12)
        readonly property bool removable: !Theme.builtInThemes.includes(modelData)

        implicitWidth: 156
        padding: 8
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        Accessible.checkable: true
        Accessible.checked: active
        onClicked: Theme.setTheme(modelData)
        TapHandler {
            enabled: card.removable
            acceptedButtons: Qt.RightButton
            onTapped: cardMenu.popup()
        }

        SettingsMenu {
            id: cardMenu
            atPointer: true
            SettingsMenuItem {
                text: qsTr("Rename")
                onTriggered: Qt.callLater(() => renameDialog.openFor("theme", card.modelData))
            }
            SettingsMenuItem {
                danger: true
                text: qsTr("Delete")
                onTriggered: Qt.callLater(() => deleteDialog.openFor("theme", card.modelData))
            }
        }

        background: Surface {
            radius: Theme.settingsWindow.cardRadius
            fill: Theme.settingsWindow.cardBackground
            fillEnd: Theme.settingsWindow.cardBackgroundEnd
            direction: Theme.controls.gradientDirection
            border.width: card.active || card.visualFocus ? 2 : Theme.settingsWindow.cardBorderWidth
            border.color: card.active || card.visualFocus ? Theme.controls.inputBorderFocus
                                                         : Theme.settingsWindow.cardBorder

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                visible: card.hovered && !card.active
                color: Theme.palette.hoverOverlay
            }
        }

        contentItem: ColumnLayout {
            spacing: 8

            Rectangle {
                id: mock
                Layout.fillWidth: true
                implicitHeight: 76
                radius: card.corner
                color: card.c.background ?? "transparent"
                clip: true

                Rectangle {
                    readonly property bool floating: card.c.railContainer ?? false
                    x: floating ? 4 : 0
                    y: floating ? 4 : 0
                    width: floating ? 24 : 28
                    height: floating ? parent.height - 8 : parent.height
                    radius: floating ? card.corner / 2 + 1 : 0
                    color: card.c.rail ?? "transparent"
                    border.width: floating ? Math.min(card.c.railBorderWidth ?? 0, 1) : 0
                    border.color: card.c.railBorder ?? "transparent"
                }
                Rectangle {
                    x: 9
                    y: 10
                    width: 14
                    height: 14
                    radius: card.corner / 2
                    color: card.c.railSelected ?? card.c.accent ?? "transparent"
                }
                Repeater {
                    model: [33.5, 51.5]
                    Rectangle {
                        required property real modelData
                        x: 12.5
                        y: modelData
                        width: 7
                        height: 7
                        radius: Theme.settingsWindow.previewDotRadius
                        color: card.c.railIcon ?? "transparent"
                        opacity: 0.4
                    }
                }
                Rectangle {
                    x: 38
                    y: 8
                    width: parent.width - 46
                    height: 38
                    radius: Math.min(card.c.radius?.large ?? 0, 16) / 2
                    color: card.c.surface ?? "transparent"
                }
                Rectangle {
                    x: 38
                    y: parent.height - 22
                    width: 34
                    height: 12
                    radius: Math.min(card.c.radius?.small ?? 0, height / 2)
                    color: card.c.launch ?? card.c.accent ?? "transparent"
                }
                IconButton {
                    visible: card.removable && (card.hovered || card.modelData === root.hoverTheme)
                    anchors { top: parent.top; right: parent.right; margins: 4 }
                    implicitWidth: 28
                    implicitHeight: 28
                    iconSize: 14
                    icon: Icons.close
                    tip: qsTr("Delete")
                    onClicked: deleteDialog.openFor("theme", card.modelData)
                }
            }

            Label {
                Layout.fillWidth: true
                text: card.modelData
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                elide: Text.ElideRight
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        clip: true
        contentHeight: content.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

        ColumnLayout {
            id: content
            width: flick.width - scrollBar.width - Theme.layout.spacing
            spacing: 0

            SectionHeader { text: qsTr("Themes") }
            Flow {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.layout.spacing
                spacing: Theme.layout.spacing

                Repeater {
                    model: Theme.availableThemes
                    ThemeCard {}
                }
            }

            SectionHeader { text: qsTr("In use") }
            SettingsCard {
                spacing: Theme.layout.spacing * 2

                LabeledRow {
                    label: qsTr("Colors")
                    note: qsTr("Every color. Edit them on the Colors page.")
                    SettingsComboBox {
                        Layout.preferredWidth: 200
                        model: Theme.availableColors
                        currentIndex: Math.max(0, Theme.availableColors.indexOf(Theme.activeColors))
                        onActivated: Theme.setColors(currentText)
                    }
                }
                LabeledRow {
                    label: qsTr("Style")
                    note: qsTr("Corners, borders, sizes, labels and layout. Edit them on the Style page.")
                    SettingsComboBox {
                        Layout.preferredWidth: 200
                        model: Theme.availableStyles
                        currentIndex: Math.max(0, Theme.availableStyles.indexOf(Theme.activeStyle))
                        onActivated: Theme.setStyle(currentText)
                    }
                }
                LabeledRow {
                    label: Theme.activeTheme !== "" ? qsTr("This is the theme %1").arg(Theme.activeTheme)
                                                    : qsTr("This pair isn't a saved theme")
                    note: qsTr("A theme is a name for a pair of colors and a style, so you can switch to the pair in one click.")
                    SettingsButton {
                        text: qsTr("Save as theme…")
                        onClicked: root.saveTheme()
                    }
                }
            }

            ProblemsButton {
                Layout.topMargin: Theme.layout.spacing
                Layout.alignment: Qt.AlignRight
                problems: Theme.problems
            }
        }
    }

    DeleteThemeDialog {
        id: deleteDialog
        onDeleted: text => root.message(text, null)
    }

    RenameThemeDialog {
        id: renameDialog
        onRenamed: text => root.message(text, null)
    }

    MotionPopup {
        id: saveDialog
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.menu.dialogPadding
        width: 320

        function openFresh() {
            saveName.text = Theme.activeTheme !== "" ? "" : Theme.activeColors + " " + Theme.activeStyle
            open()
            saveName.forceActiveFocus()
        }

        function save(replace) {
            if (Theme.saveTheme(saveName.text, replace)) {
                root.message(qsTr("Saved the theme %1").arg(saveName.text), null)
                close()
            }
        }

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
                text: qsTr("Save as theme")
                color: Theme.controls.labelColor
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("%1 colors with the %2 style").arg(Theme.activeColors).arg(Theme.activeStyle)
                color: Theme.controls.descriptionColor
                wrapMode: Text.WordWrap
            }

            Label { text: qsTr("Name"); color: Theme.controls.descriptionColor }
            SettingsTextField {
                id: saveName
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                onAccepted: if (saveButton.enabled) saveButton.clicked()
            }
            Label {
                readonly property string error: Theme.nameError("theme", saveName.text)
                visible: error !== ""
                Layout.fillWidth: true
                text: error
                wrapMode: Text.WordWrap
                color: Theme.controls.descriptionColor
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.controls.controlSpacing
                SettingsButton { text: qsTr("Cancel"); onClicked: saveDialog.close() }
                SettingsButton {
                    id: saveButton
                    primary: true
                    text: qsTr("Save")
                    enabled: Theme.nameError("theme", saveName.text) === ""
                    onClicked: Theme.themeExists(saveName.text) ? replaceDialog.open() : saveDialog.save(false)
                }
            }
        }
    }

    MotionPopup {
        id: replaceDialog
        anchors.centerIn: parent
        modal: true
        Overlay.modal: Scrim {}
        padding: Theme.menu.dialogPadding
        width: 320

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
                text: qsTr("Replace %1?").arg(saveName.text)
                color: Theme.controls.labelColor
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("There is already a theme with this name. It will name %1 colors and the %2 style instead.")
                          .arg(Theme.activeColors).arg(Theme.activeStyle)
                wrapMode: Text.WordWrap
                color: Theme.controls.descriptionColor
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.controls.controlSpacing
                SettingsButton { text: qsTr("Cancel"); onClicked: replaceDialog.close() }
                SettingsButton {
                    primary: true
                    text: qsTr("Replace")
                    onClicked: {
                        replaceDialog.close()
                        saveDialog.save(true)
                    }
                }
            }
        }
    }
}
