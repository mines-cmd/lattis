import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("How the launcher starts and behaves while a game runs, your data folder, and About.")

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    Component.onCompleted: General.reload()
    onVisibleChanged: if (visible) General.reload()

    signal runSetupGuide()
    signal message(string text)

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
                color: Theme.settingsWindow.labelColor
                font.pixelSize: Theme.settingsWindow.labelFontSize
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        ProblemsButton {
            Layout.alignment: Qt.AlignRight
            problems: General.problems.concat(GameRunner.problems)
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
                width: flick.width - scrollBar.width - Theme.layout.spacing
                spacing: 0

                SectionHeader { text: qsTr("Startup") }
                SettingsCard {
                    spacing: Theme.layout.spacing * 2
                    LabeledRow {
                        label: qsTr("Start in fullscreen")
                        note: Actions.keys.toggle_fullscreen.length > 0
                              ? qsTr("%1 leaves fullscreen.").arg(Actions.label(Actions.keys.toggle_fullscreen)) : ""
                        SettingsSwitch {
                            checked: General.startFullscreen
                            onToggled: {
                                const err = General.setStartFullscreen(checked)
                                if (err !== "") error.text = err
                            }
                        }
                    }
                    LabeledRow {
                        label: qsTr("Use system locale")
                        note: qsTr("Turn off to show month names, weekdays, dates and numbers in English. Applies at the next start.")
                        SettingsSwitch {
                            checked: General.systemLocale
                            onToggled: {
                                const err = General.setSystemLocale(checked)
                                if (err !== "") error.text = err
                            }
                        }
                    }
                    LabeledRow {
                        label: qsTr("Start at login")
                        note: General.startAtLogin && !General.startAtLoginOurs
                              ? qsTr("Lattis didn't create this login entry; turning this off removes it.")
                              : qsTr("Opens Lattis when you log in.")
                        SettingsSwitch {
                            checked: General.startAtLogin
                            onToggled: {
                                const err = General.setStartAtLogin(checked)
                                if (err !== "") error.text = err
                            }
                        }
                    }
                }

                SectionHeader { text: qsTr("Motion") }
                SettingsCard {
                    spacing: Theme.layout.spacing * 2
                    LabeledRow {
                        label: qsTr("Reduce motion")
                        note: qsTr("Turns off animations whatever the style says.")
                        SettingsSwitch {
                            checked: General.reduceMotion
                            onToggled: {
                                const err = General.setReduceMotion(checked)
                                if (err !== "") error.text = err
                            }
                        }
                    }
                }

                SectionHeader { text: qsTr("While a game runs") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Play time and last played are recorded while the launcher runs.")
                }
                SettingsCard {
                    spacing: Theme.layout.spacing * 2
                    LabeledRow {
                        label: qsTr("Launcher window")
                        note: !WindowState.canMinimize && GameRunner.minimizeWhileRunning
                              ? qsTr("This desktop can't minimize windows, so the launcher stays open.") : ""
                        SettingsComboBox {
                            model: [qsTr("Keep the launcher open"), qsTr("Minimize the launcher")]
                            currentIndex: GameRunner.minimizeWhileRunning ? 1 : 0
                            onActivated: {
                                const value = currentIndex === 1 ? "minimize" : "stay"
                                const err = GameRunner.setLaunchValue("while_running", value)
                                if (err !== "") error.text = err
                            }
                        }
                    }
                    LabeledRow {
                        label: qsTr("Minimum session length")
                        note: qsTr("Shorter sessions change neither play time nor last played.")
                        SettingsTextField {
                            id: sessionField
                            implicitWidth: 80
                            implicitHeight: Theme.settingsWindow.inputHeight
                            rightPadding: unit.width + 2 * Theme.layout.spacing
                            text: GameRunner.minSessionSeconds.toString()
                            horizontalAlignment: TextInput.AlignRight
                            inputMethodHints: Qt.ImhDigitsOnly
                            onTextChanged: saveTimer.restart()
                            onEditingFinished: { saveTimer.stop(); root.saveSession() }

                            Label {
                                id: unit
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.layout.spacing
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("s")
                                color: Theme.settingsWindow.descriptionColor
                                font.pixelSize: sessionField.font.pixelSize
                            }
                        }
                    }
                }

                SectionHeader { text: qsTr("Library") }
                SettingsNote {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.layout.spacing
                    text: qsTr("Your games, art, themes and settings live in the data folder; Back up library saves it.")
                }
                SettingsCard {
                    spacing: Theme.layout.spacing * 2
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacing / 2
                        Label {
                            text: qsTr("Data folder")
                            color: Theme.settingsWindow.labelColor
                            font.pixelSize: Theme.settingsWindow.labelFontSize
                        }
                        PathField {
                            Layout.fillWidth: true
                            path: General.dataDir
                            onCopied: root.message(qsTr("Copied"))

                            SettingsButton {
                                text: qsTr("Open")
                                tip: qsTr("Open the data folder in the file manager")
                                onClicked: GameRunner.openDataFolder()
                            }
                        }
                        SettingsNote {
                            Layout.fillWidth: true
                            visible: General.dataDirFromEnv
                            text: qsTr("Set by the LATTIS_DATA environment variable.")
                        }
                    }
                    LabeledRow {
                        label: qsTr("Setup guide")
                        note: qsTr("Steam, emulators and ROM folders, game info keys, and the theme, step by step.")
                        SettingsButton {
                            text: qsTr("Run setup guide")
                            onClicked: root.runSetupGuide()
                        }
                    }
                }

                SectionHeader { text: qsTr("About") }
                SettingsCard {
                    AboutContent {
                        Layout.fillWidth: true
                        logoHeight: Theme.about.pageLogoHeight
                        backgroundColor: Theme.settingsWindow.cardBackground
                        labelColor: Theme.settingsWindow.labelColor
                        linkColor: Theme.palette.accent
                    }
                }

                Item { Layout.preferredHeight: Theme.settingsWindow.sectionSpacing / 2 }
            }
        }

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.settingsWindow.problemsText
            font.pixelSize: Theme.settingsWindow.descriptionFontSize
            wrapMode: Text.WordWrap
        }
    }

    Timer {
        id: saveTimer
        interval: 500
        onTriggered: saveSession()
    }

    function saveSession() {
        const val = parseInt(sessionField.text)
        if (isNaN(val) || val < 0 || val > 3600) {
            sessionField.text = GameRunner.minSessionSeconds.toString()
            return
        }
        const err = GameRunner.setLaunchValue("min_session_seconds", val.toString())
        if (err !== "") error.text = err
    }
}
